#include "HttpClient.h"
#include <boost/log/trivial.hpp>
#include <memory.h>

const char * HttpClient::FORMAT_CERT = "PEM";
const char * HttpClient::FORMAT_KEY = "PEM";

HttpClient::HttpClient(string url_)
{
	this->url = url_;
	this->header_http = "";
	this->string_post = "";

	this->flag_communication_error = false;
	this->string_communication_error = "";

	this->timeout = 60;
	this->multipart_post = NULL;

	this->buf_response_callback = NULL;
	this->size_response_callback = 0;
	this->headerlist = NULL;
	this->response = "";
	this->code_response = -1;
	this->curl_return_code = -1;
}

HttpClient::~HttpClient()
{
	if(this->buf_response_callback) free(this->buf_response_callback);
}

string HttpClient::getStringError()
{
	return this->string_communication_error;
}

void HttpClient::setURL(string url_)
{
	this->url = "";
	this->url.append(url_);
}

void HttpClient::setPostString(string string_post_)
{
	this->string_post = "";
	this->string_post.append(string_post_);
}

void HttpClient::setSSLCert(string path_cert_, string path_private_key_, string pass_private_key_)
{
	this->path_cert = "";
	this->path_cert.append(path_cert_);
	this->path_private_key = "";
	this->path_private_key.append(path_private_key_);
	this->pass_private_key = "";
	this->pass_private_key.append(pass_private_key_);
}

void HttpClient::execute(void)
{
	CURL *curl;
	CURLcode res;

	curl = curl_easy_init();

	if(curl) {
		this->setCurlEasy(curl);
		BOOST_LOG_TRIVIAL(debug) << "http before perform";
		res = curl_easy_perform(curl);
		BOOST_LOG_TRIVIAL(debug) << "http after perform";
		if(res != CURLE_OK){
			this->string_communication_error = curl_easy_strerror(res);
			BOOST_LOG_TRIVIAL(debug) << "CURL_NG";
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		}else{
			if(this->size_response_callback > 0) this->response = string(this->buf_response_callback);
		}
	
		this->curl_return_code = (int)res;
		long code;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
		this->code_response = (int)code;
		makeCookie(curl);
		curl_easy_cleanup(curl);
		if(this->headerlist) curl_slist_free_all(this->headerlist);
	}
}

void HttpClient::setCurlEasy(CURL* curl)
{
	curl_easy_setopt(curl, CURLOPT_URL, this->url.c_str());
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, this->timeout);

	//header
	this->headerlist = NULL;
	this->headerlist = curl_slist_append(this->headerlist, "Expect:");
	this->headerlist = curl_slist_append(this->headerlist, this->header_http.c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, this->headerlist);

	//cookie
	if(this->cookie_set.length() > 0){
		int length = this->cookie_set.length();
		const char *cc = this->cookie_set.c_str();
		int start = 0;
		for(int i=0;i<length;++i){
			if(*(cc+i) == '\n'){
				string s(this->cookie_set, start, i-start-1);
				start = i+1;
				//printf("%s\n", s.c_str());
				curl_easy_setopt(curl, CURLOPT_COOKIELIST, s.c_str());
			}
		}
	}else{
		curl_easy_setopt(curl, CURLOPT_COOKIELIST, "");
	}

	//ssl
	curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, HttpClient::FORMAT_CERT);
	curl_easy_setopt(curl, CURLOPT_SSLCERT, this->path_cert.c_str());
	curl_easy_setopt(curl, CURLOPT_SSLKEYPASSWD, this->pass_private_key.c_str());
	curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, HttpClient::FORMAT_KEY);
	curl_easy_setopt(curl, CURLOPT_SSLKEY, this->path_private_key.c_str());
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

	if(this->multipart_post){
		//multipart
		curl_easy_setopt(curl, CURLOPT_POST, 1);
		curl_easy_setopt(curl, CURLOPT_HTTPPOST, this->multipart_post);
	}else if(this->string_post.length() > 0){
		//string
		curl_easy_setopt(curl, CURLOPT_POST, 1);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, this->string_post.c_str());
		//post field size. error on -1. strange internal strlen()?
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, this->string_post.length());
	}
	if(this->custom_request.length() > 0){
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, this->custom_request.c_str());
	}

	//response callback
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, HttpClient::writeMemoryCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)this);

	//for anti-bug longjump causes
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);

	//debug
	//curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
	//curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, debugCallback);
}

size_t HttpClient::writeMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t realsize = size*nmemb;
	HttpClient *thisa = (HttpClient *)userp;

  thisa->buf_response_callback = (char *)realloc(thisa->buf_response_callback, thisa->size_response_callback + realsize + 1);
  if(thisa->buf_response_callback == NULL) {
    //out of memory!
	  BOOST_LOG_TRIVIAL(debug) << "not enough memory (realloc returned NULL)";
    return 0;
  }

  memcpy(&(thisa->buf_response_callback[thisa->size_response_callback]), contents, realsize);
  thisa->size_response_callback += realsize;
  thisa->buf_response_callback[thisa->size_response_callback] = 0;

  return realsize;
}

int HttpClient::debugCallback(CURL* curl_handle, curl_infotype infotype, char* chr, size_t size, void* unused)
{
    size_t i, written;
    char* c = chr;

    (void)sizeof(unused);

    if (!curl_handle || !chr)
    {
        return -1;
    }

    //fprintf(stderr, "\n\n");

    bool flag_show = false;
    switch (infotype)
    {
    /*
        case CURLINFO_TEXT:
            fprintf(stderr, "CURL-debug: Informational data\n");
            fprintf(stderr, "------------------------------\n");
            break;

        case CURLINFO_HEADER_IN:
            fprintf(stderr, "CURL-debug: Incoming header\n");
            fprintf(stderr, "---------------------------\n");
            break;
	*/
        case CURLINFO_HEADER_OUT:
            fprintf(stderr, "CURL-debug: Outgoing header\n");
            fprintf(stderr, "---------------------------\n");
            	flag_show = true;
            break;
/*
        case CURLINFO_DATA_IN:
            fprintf(stderr, "CURL-debug: Incoming data\n");
            fprintf(stderr, "-------------------------\n");
            break;
*/
        case CURLINFO_DATA_OUT:
            fprintf(stderr, "CURL-debug: Outgoing data\n");
            fprintf(stderr, "-------------------------\n");
            	flag_show = true;
            break;
        default:
            //fprintf(stderr, "CURL-debug: Unknown\n");
            //fprintf(stderr, "-------------------\n");
            break;
    }
    if(!flag_show) return 0;

    written = 0;
    for (i = 0; i < size; i++)
    {
        written++;
        if (isprint(*c))
        {
            fprintf(stderr,"%c", *c);
        }
        else if (*c == '\n')
        {
            fprintf(stderr, "\n");
            written = 0;
        }
        else
        {
            fprintf(stderr, ".");
        }

        if (written == 70)
        {
            fprintf(stderr, "\n");
        }
        c++;
    }
    return 0;
}

void HttpClient::makeCookie(CURL *curl)
{
	this->cookie_got = "";
	struct curl_slist *cookies;
	curl_easy_getinfo(curl, CURLINFO_COOKIELIST, &cookies);
	if(cookies){
		struct curl_slist *nc = cookies;
		while (nc) {
			this->cookie_got.append(nc->data);
			this->cookie_got.append("\n");
			nc = nc->next;
		}
	}

	curl_slist_free_all(cookies);
}
