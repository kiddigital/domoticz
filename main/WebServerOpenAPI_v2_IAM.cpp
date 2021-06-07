/******************************************************************************
  All '/IAM' calls of OpenAPI_v2 Webserver are here

  This file gets included by the main class from 'WebServerOpenAPI_v2.cpp'
  All method definitions are in the main header file 'WebServerOpenAPI_v2.h'
******************************************************************************/

void CWebServerOpenAPI_v2::PostIAMAuthorize(const Json::Value& input, Json::Value& result)
{
	result["status"] = TESTDEFINE;
	result["params"] = input;
}

void CWebServerOpenAPI_v2::PostIAMToken(const Json::Value& input, Json::Value& result)
{
	result["status"] = TESTDEFINE;
	result["params"] = input;
}

void CWebServerOpenAPI_v2::GetIAMUserinfo(const Json::Value& input, Json::Value& result)
{
	result["status"] = TESTDEFINE;
	result["params"] = input;
}

void CWebServerOpenAPI_v2::PostIntrospect(const Json::Value& input, Json::Value& result)
{
	result["status"] = TESTDEFINE;
	result["params"] = input;
}
