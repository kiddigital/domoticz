/******************************************************************************
  All '/Services' calls of OpenAPI_v2 Webserver are here

  This file gets included by the main class from 'WebServerOpenAPI_v2.cpp'
  All method definitions are in the main header file 'WebServerOpenAPI_v2.h'
******************************************************************************/

void CWebServerOpenAPI_v2::GetServicesVersion(const Json::Value& input, Json::Value& result)
{
  result["version"] = "hidden";

  if (m_userrights != (int8_t)http::server::URIGHTS_NONE)
  {
    result["version"] = szAppVersion;
    result["hash"] = szAppHash;
    result["build_time"] = szAppDate;
    //CdzVents* dzvents = CdzVents::GetInstance();
    //result["dzvents_version"] = dzvents->GetVersion();
    result["python_version"] = szPyVersion;
    result["UseUpdate"] = false;
    result["HaveUpdate"] = m_mainworker.IsUpdateAvailable(false);

    if (m_userrights == http::server::URIGHTS_ADMIN)
    {
      result["UseUpdate"] = g_bUseUpdater;
      result["DomoticzUpdateURL"] = m_mainworker.m_szDomoticzUpdateURL;
      result["SystemName"] = m_mainworker.m_szSystemName;
      result["Revision"] = m_mainworker.m_iRevision;
    }
  }
}

void CWebServerOpenAPI_v2::GetServicesStatus(const Json::Value& input, Json::Value& result)
{
	result["status"] = TESTDEFINE;
	result["params"] = input;
}
