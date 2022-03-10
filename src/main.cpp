#include <iostream>
#include <filesystem>
#include <string>
#include <set>
#include <optional>
#include <fstream>
#include <sstream>


#include <nlohmann/json.hpp>
#include <nlohmann/json-schema.hpp>
#include <cpr/cpr.h>
#include <base64.hpp>
#include <cxxopts.hpp>

#include <test.h>

// for convenience
using json = nlohmann::json;
namespace json_schema = nlohmann::json_schema;
namespace fs = std::filesystem;

using std::string;
using std::u8string;
using std::set;
using std::optional;
using std::pair;

struct opt_t{
    bool base64 = true;
    bool log = true;
};

pair<optional<json>,optional<json>> parse_resource_helper(const string& base, const string& pname, const opt_t& opt){
    auto [base64,log]=opt;

    if(log)std::cerr<<"Parse Step @"<<base<<"/"<<pname<<"\n";

    string name = base64?base64::to_base64(pname):pname;

    optional<fs::path> file_schema;     // name.schema

    optional<fs::path> file_ctnt;       // name.data
    optional<fs::path> file_json;       // name.json
    optional<fs::path> file_bson;       // name.bson

    enum {NONE,JSON,BSON,CONTENT} mode_t = NONE;

    if(log)std::cerr<<"· Checking resources:\n";


    {
        std::error_code ec;
        fs::path tmp = base+"/"+name+".schema";
        if(fs::is_regular_file(tmp,ec)){
            if(log)std::cerr<<"  ✓ Schema\n";
            file_schema=tmp;
        }
        else file_schema=std::nullopt;
    }

    {
        std::error_code ec;
        fs::path tmp = base+"/"+name+".data";
        if(fs::is_regular_file(tmp,ec)){
            if(log)std::cerr<<"  ✓ Data\n";
            file_ctnt=tmp;
            mode_t = CONTENT;

        }
        else{
            file_ctnt=std::nullopt;
        }
    }

    {
        std::error_code ec;
        fs::path tmp = base+"/"+name+".json";
        if(fs::is_regular_file(tmp,ec)){
            if(log)std::cerr<<"  ✓ JSON\n";
            file_json=tmp;
            mode_t = JSON;
        }
        else{
            file_json=std::nullopt;
        }
    }

    {
        std::error_code ec;
        fs::path tmp = base+"/"+name+".bson";
        if(fs::is_regular_file(tmp,ec)){
            if(log)std::cerr<<"  ✓ BSON\n";
            file_bson=tmp;
            mode_t = BSON;
        }
        else{
            file_bson=std::nullopt;
        }
    }

    uint source_check = file_ctnt.has_value() + file_json.has_value() + file_bson.has_value();

    if(source_check == 0) mode_t = NONE;
    if(source_check > 1){
        if(log)std::cerr<<"  ✕ Multiple conflicting sources\n";
        throw "Multiple Subtree Sources";
    }

    //Returning values
    optional<json> tmp_json, tmp_schema;

    // Load schema:
    if(!file_schema) tmp_schema = std::nullopt;
    else{
        if(log)std::cerr<<"· Loading schema ";
        std::ifstream i(file_schema.value(), std::ios::binary);
        tmp_schema = "{}"_json;
        i >> tmp_schema.value();
        if(log)std::cerr<<" ✓\n";
    }

    // Load "content"
    if(mode_t == NONE){
        tmp_json = std::nullopt;
    }
    else if(mode_t == CONTENT){
        if(log)std::cerr<<"· Loading content (data)";
        //Please solve this with streams to avoid all data to be loaded at any time.
        std::ifstream t(file_ctnt.value(),std::ios::binary);
        std::stringstream buffer;
        buffer << t.rdbuf();
        tmp_json = base64::to_base64(buffer.str()) ;// Load content
        if(log)std::cerr<<" ✓\n";
    }
    else if (mode_t == JSON){
        if(log)std::cerr<<"· Loading content (JSON)";
        std::ifstream i(file_json.value(), std::ios::binary);
        tmp_json = "{}"_json;
        i >> tmp_json.value();
        if(log)std::cerr<<" ✓\n";
    }
    else if (mode_t == BSON){
        if(log)std::cerr<<"· Loading content (BSON)";
        std::ifstream i(file_bson.value(), std::ios::binary);
        tmp_json = "{}"_json;
        i >> tmp_json.value();
        if(log)std::cerr<<" ✓\n";
    }
    else{
        if(log)std::cerr<<"  ✕ Unknown source type!\n";
        throw "What?";
    }

    // Recursive application. Check each field and expand.
    if(tmp_json)
    for(auto& [field,value]:tmp_json.value().items()){
        if(log)std::cerr<<"· Testing child:\n";
        auto [rjson,rschema] = parse_resource_helper(base+"/"+name, field, opt);
        if(rjson){
            // Check schema
            if(rschema){
                if(log)std::cerr<<"  · Testing schema";
                json_schema::json_validator validator;
                validator.set_root_schema(rschema.value());
                validator.validate(rjson.value());
                if(log)std::cerr<<" ✓\n";    
            }
            if(log)std::cerr<<"  · Child substitution";
            // Apply
            tmp_json.value()[field]=rjson.value();
            if(log)std::cerr<<" ✓\n";

        }
    }
    return {tmp_json, tmp_schema};

}

optional<json> parse_resource(const string& dir, const string& filename, const opt_t& opt){
    auto [tree,schema] = parse_resource_helper(dir,filename, opt);

    if(!tree)return std::nullopt;
    if(schema){
        json_schema::json_validator validator;
        validator.set_root_schema(schema.value());
        validator.validate(tree.value());    
    }

    return tree.value();
}

int main(int argc, char** argv) {
    cxxopts::Options options("json-stitcher", "Output the full JSON file from a fragmented source.");

    try{
        options.add_options()
            ("v,verbose", "Enable verbose mode for locating issues.", cxxopts::value<bool>()->default_value("false")) // a bool parameter
            ("b,base64", "Enable the base64 representation of keys for JSON files using characters not supported by the filesystem.", cxxopts::value<bool>()->default_value("false"))
            ("f,file", "Root of the JSON file (no extension).", cxxopts::value<std::string>())
            ("p,pretty", "Prettify the output.", cxxopts::value<bool>()->default_value("false"))
            ("h,help", "Show some help.")
            ;

        auto result = options.parse(argc, argv);

        if (result.count("help")){
            std::cout << options.help() << std::endl;
            exit(0);
        }

        string p = result["file"].as<string>();
        string path = (p.rfind("/")!=-1)?(p.substr(0,p.rfind("/"))):"";
        string name = p.substr(p.rfind("/") + 1);

        //std::cerr<<"Path: "<<path<<"; Name: "<<name<<"\n";
        auto tree = parse_resource(path, name, {result["base64"].as<bool>(),result["verbose"].as<bool>()});
        if(result["pretty"].as<bool>()==true){
            std::cout<<std::setw(4)<<(tree?tree.value():"")<<std::endl;;
        }
        else{
            std::cout<<(tree?tree.value():"")<<std::endl;;  
        }
        return 0;

    }
    catch(cxxopts::OptionException& ev){
        std::cerr<<ev.what()<<std::endl;
        std::cerr<<options.help()<<std::endl;
        exit(1);
    }

}


/*    cpr::Response r = cpr::Get(cpr::Url{"https://api.github.com/repos/whoshuu/cpr/contributors"},
                      cpr::Authentication{"user", "pass"},
                      cpr::Parameters{{"anon", "true"}, {"key", "value"}});
    r.status_code;                  // 200
    r.header["content-type"];       // application/json; charset=utf-8
    r.text;                         // JSON text string
    std::cout<<r.text<<std::endl;
    */