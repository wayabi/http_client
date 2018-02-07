#include "HttpClient.h"
#include "json11.hpp"
#include <string>
#include <boost/log/trivial.hpp>

using namespace std;

int main()
{
	HttpClient client("http://acchub.net/summary/ticktock");
	client.execute();
	string res = client.getStringResponse();
	string err;
	json11::Json j = json11::Json::parse(res, err);
	int num_client = j["num_client"].int_value();
	int shake = j["shake"].int_value();
	double hz = j["hz"].number_value();
	double power = j["power"].number_value();
	BOOST_LOG_TRIVIAL(info) << "num_client:" << num_client;
	BOOST_LOG_TRIVIAL(info) << "shake:" << shake;
	BOOST_LOG_TRIVIAL(info) << "hz:" << hz;
	BOOST_LOG_TRIVIAL(info) << "power:" << power;
	return 0;
}
