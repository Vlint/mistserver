#include "output_httpts.h"
#include <mist/defines.h>
#include <mist/http_parser.h>
#include <mist/procs.h>
#include <mist/stream.h>
#include <mist/url.h>
#include <unistd.h>

namespace Mist{
  OutHTTPTS::OutHTTPTS(Socket::Connection &conn) : TSOutput(conn){
    sendRepeatingHeaders = 500; // PAT/PMT every 500ms (DVB spec)

    if (config->getString("target").substr(0, 6) == "srt://"){
      std::string tgt = config->getString("target");
      HTTP::URL srtUrl(tgt);
      config->getOption("target", true).append("ts-exec:srt-live-transmit file://con " + srtUrl.getUrl());
      INFO_MSG("Rewriting SRT target '%s' to '%s'", tgt.c_str(), config->getString("target").c_str());
    }
    if (config->getString("target").substr(0, 8) == "ts-exec:"){
      std::string input = config->getString("target").substr(8);
      char *args[128];
      uint8_t argCnt = 0;
      char *startCh = 0;
      for (char *i = (char *)input.c_str(); i <= input.data() + input.size(); ++i){
        if (!*i){
          if (startCh){args[argCnt++] = startCh;}
          break;
        }
        if (*i == ' '){
          if (startCh){
            args[argCnt++] = startCh;
            startCh = 0;
            *i = 0;
          }
        }else{
          if (!startCh){startCh = i;}
        }
      }
      args[argCnt] = 0;

      int fin = -1;
      Util::Procs::StartPiped(args, &fin, 0, 0);
      myConn.open(fin, -1);

      wantRequest = false;
      parseData = true;
    }
  }

  OutHTTPTS::~OutHTTPTS(){}

  void OutHTTPTS::initialSeek(){
    // Adds passthrough support to the regular initialSeek function
    if (targetParams.count("passthrough")){selectAllTracks();}
    Output::initialSeek();
  }

  void OutHTTPTS::init(Util::Config *cfg){
    HTTPOutput::init(cfg);
    capa["name"] = "HTTPTS";
    capa["friendly"] = "TS over HTTP";
    capa["desc"] = "Pseudostreaming in MPEG2/TS format over HTTP";
    capa["url_rel"] = "/$.ts";
    capa["url_match"] = "/$.ts";
    capa["socket"] = "http_ts";
    capa["codecs"][0u][0u].append("+H264");
    capa["codecs"][0u][0u].append("+HEVC");
    capa["codecs"][0u][0u].append("+MPEG2");
    capa["codecs"][0u][1u].append("+AAC");
    capa["codecs"][0u][1u].append("+MP3");
    capa["codecs"][0u][1u].append("+AC3");
    capa["codecs"][0u][1u].append("+MP2");
    capa["codecs"][0u][1u].append("+opus");
    capa["methods"][0u]["handler"] = "http";
    capa["methods"][0u]["type"] = "html5/video/mpeg";
    capa["methods"][0u]["priority"] = 1;
    capa["push_urls"].append("/*.ts");
    capa["push_urls"].append("ts-exec:*");

#ifndef WITH_SRT
    {
      pid_t srt_tx = -1;
      const char *args[] ={"srt-live-transmit", 0};
      srt_tx = Util::Procs::StartPiped(args, 0, 0, 0);
      if (srt_tx > 1){
        capa["push_urls"].append("srt://*");
        capa["desc"] = capa["desc"].asStringRef() +
                       ". Non-native SRT push output support (srt://*) is installed and available.";
      }else{
        capa["desc"] =
            capa["desc"].asStringRef() +
            ". To enable non-native SRT push output support, please install the srt-live-transmit binary.";
      }
    }
#endif

    JSON::Value opt;
    opt["arg"] = "string";
    opt["default"] = "";
    opt["arg_num"] = 1;
    opt["help"] = "Target filename to store TS file as, or - for stdout.";
    cfg->addOption("target", opt);
  }

  bool OutHTTPTS::isRecording(){return config->getString("target").size();}

  void OutHTTPTS::onHTTP(){
    std::string method = H.method;
    initialize();
    H.clearHeader("Range");
    H.clearHeader("Icy-MetaData");
    H.clearHeader("User-Agent");
    H.clearHeader("Host");
    H.clearHeader("Accept-Ranges");
    H.clearHeader("transferMode.dlna.org");
    H.SetHeader("Content-Type", "video/mpeg");
    H.setCORSHeaders();
    if (method == "OPTIONS" || method == "HEAD"){
      H.SendResponse("200", "OK", myConn);
      H.Clean();
      return;
    }
    H.protocol = "HTTP/1.0"; // Force HTTP/1.0 because some devices just don't understand chunked replies
    H.StartResponse(H, myConn);
    parseData = true;
    wantRequest = false;
  }

  void OutHTTPTS::sendTS(const char *tsData, size_t len){
    if (isRecording()){
      myConn.SendNow(tsData, len);
      return;
    }
    H.Chunkify(tsData, len, myConn);
    if (targetParams.count("passthrough")){selectAllTracks();}
  }
}// namespace Mist
