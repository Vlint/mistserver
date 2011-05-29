#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <stdlib.h>
#include <string>
#include <vector>

#include "stream.h"

class GB_Client {
  public:
    GB_Client( );
    ~GB_Client( );
    void Parse_Config( );
  private:
    std::string ReadConfig( FILE * File );
    void Parse( );
    std::string MyName;
    std::vector<std::string> StreamNames;
    std::vector<Stream> Streams;
    std::string ConfigFile;
};//GB_Client

