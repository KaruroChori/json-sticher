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
    bool links = true;
    bool recursive = true;
};

pair<optional<json>,optional<json>> parse_resource_helper(const string& base, const string& pname, json& myself, const opt_t& opt);
optional<json> parse_resource(const string& dir, const string& filename, const opt_t& opt);
optional<json> parse_resource(const string& file, const opt_t& opt);

pair<optional<json>,optional<json>> parse_resource_helper(const string& base, const string& pname, json& myself, const opt_t& opt){
    if(opt.log)std::cerr<<"Parse Step @"<<base<<"/"<<pname<<"\n";

    string name = opt.base64?base64::to_base64(pname):pname;

    optional<fs::path> file_schema;     // name.schema

    optional<fs::path> file_ctnt;       // name.data
    optional<fs::path> file_json;       // name.json
    optional<fs::path> file_bson;       // name.bson

    enum {NONE,JSON,BSON,L_JSON,L_BSON,CONTENT} mode_t = NONE;

    if(opt.log)std::cerr<<"· Checking resources:\n";


    {
        std::error_code ec;
        fs::path tmp = base+"/"+name+".schema";
        if(fs::is_regular_file(tmp,ec)){
            if(opt.log)std::cerr<<"  ✓ Schema\n";
            file_schema=tmp;
        }
        else file_schema=std::nullopt;
    }

    {
        std::error_code ec;
        fs::path tmp = base+"/"+name+".data";
        if(fs::is_regular_file(tmp,ec)){
            if(opt.log)std::cerr<<"  ✓ Data\n";
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
        if(opt.links && fs::is_symlink(tmp,ec)){
            if(opt.log)std::cerr<<"  ✓ JSON↗\n";
            file_json=tmp;
            mode_t = L_JSON;
        }
        else if(fs::is_regular_file(tmp,ec)){
            if(opt.log)std::cerr<<"  ✓ JSON\n";
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
        if(opt.links && fs::is_symlink(tmp,ec)){
            if(opt.log)std::cerr<<"  ✓ BSON↗\n";
            file_bson=tmp;
            mode_t = L_BSON;
        }
        else if(fs::is_regular_file(tmp,ec)){
            if(opt.log)std::cerr<<"  ✓ BSON\n";
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
        if(opt.log)std::cerr<<"  ✕ Multiple conflicting sources\n";
        throw "Multiple Subtree Sources";
    }

    //Returning values
    optional<json> tmp_json, tmp_schema;

    // Load schema:
    if(!file_schema) tmp_schema = std::nullopt;
    else{
        if(opt.log)std::cerr<<"· Loading schema ";
        std::ifstream i(file_schema.value(), std::ios::binary);
        tmp_schema = "{}"_json;
        i >> tmp_schema.value();
        if(opt.log)std::cerr<<" ✓\n";
    }

    // Load "content"
    if(mode_t == NONE){
        if(opt.recursive && myself.is_object())tmp_json = myself;
        else tmp_json = std::nullopt;
    }
    else if(mode_t == CONTENT){
        if(opt.log)std::cerr<<"· Loading content (data)";
        //Please solve this with streams to avoid all data to be loaded at any time.
        std::ifstream t(file_ctnt.value(),std::ios::binary);
        std::stringstream buffer;
        buffer << t.rdbuf();
        tmp_json = base64::to_base64(buffer.str()) ;// Load content
        if(opt.log)std::cerr<<" ✓\n";
    }
    else if (mode_t == JSON){
        if(opt.log)std::cerr<<"· Loading content (JSON)";
        std::ifstream i(file_json.value(), std::ios::binary);
        tmp_json = "{}"_json;
        i >> tmp_json.value();
        if(opt.log)std::cerr<<" ✓\n";
    }
    else if (mode_t == BSON){
        if(opt.log)std::cerr<<"· Loading content (BSON)";
        std::ifstream i(file_bson.value(), std::ios::binary);
        tmp_json = "{}"_json;
        i >> tmp_json.value();
        if(opt.log)std::cerr<<" ✓\n";
    }
    else if (mode_t == L_JSON){
        string file=fs::canonical(file_json.value());
        if(opt.log)std::cerr<<"· Loading content (JSON↗)";
        //Strip extension
        std::cout<<((file.rfind(".")!=-1)?(file.substr(0,file.rfind("."))):"")<<"\n\n";
        tmp_json = parse_resource((file.rfind(".")!=-1)?(file.substr(0,file.rfind("."))):"",opt);
        //if(opt.log)std::cerr<<" ✓\n";
    }
    else if (mode_t == L_BSON){
        string file=fs::canonical(file_bson.value());
        if(opt.log)std::cerr<<"· Loading content (BSON↗)";
        //Strip extension
        std::cout<<((file.rfind(".")!=-1)?(file.substr(0,file.rfind("."))):"")<<"\n\n";
        tmp_json = parse_resource((file.rfind(".")!=-1)?(file.substr(0,file.rfind("."))):"",opt);
        //if(opt.log)std::cerr<<" ✓\n";
    }
    else{
        if(opt.log)std::cerr<<"  ✕ Unknown source type!\n";
        throw "What?";
    }

    // Recursive application. Check each field and expand.
    if(tmp_json)
    for(auto& [field,value]:tmp_json.value().items()){
        if(opt.log)std::cerr<<"· Testing child {"<<field<<"}:\n";
        auto [rjson,rschema] = parse_resource_helper(base+"/"+name, field, value, opt);
        if(rjson){
            // Check schema
            if(rschema){
                if(opt.log)std::cerr<<"  · Testing schema";
                json_schema::json_validator validator;
                validator.set_root_schema(rschema.value());
                validator.validate(rjson.value());
                if(opt.log)std::cerr<<" ✓\n";    
            }
            if(opt.log)std::cerr<<"  · Child substitution";
            // Apply
            tmp_json.value()[field]=rjson.value();
            if(opt.log)std::cerr<<" ✓\n";

        }
    }
    return {tmp_json, tmp_schema};

}

optional<json> parse_resource(const string& dir, const string& filename, const opt_t& opt){
    json empty = {};
    auto [tree,schema] = parse_resource_helper(dir,filename, empty, opt);

    if(!tree)return std::nullopt;
    if(schema){
        json_schema::json_validator validator;
        validator.set_root_schema(schema.value());
        validator.validate(tree.value());    
    }

    return tree.value();
}

optional<json> parse_resource(const string& file, const opt_t& opt){
    string path = (file.rfind("/")!=-1)?(file.substr(0,file.rfind("/"))):"";
    string name = file.substr(file.rfind("/") + 1);
    return parse_resource(path,name,opt);
}

int main(int argc, char** argv) {
    cxxopts::Options options("json-stitcher", "Output the full JSON file from a fragmented source.");

    try{
        options.add_options()
            ("v,verbose", "Enable verbose mode for locating issues.", cxxopts::value<bool>()->default_value("false")) // a bool parameter
            ("b,base64", "Enable the base64 representation of keys for JSON files using characters not supported by the filesystem.", cxxopts::value<bool>()->default_value("false"))
            ("f,file", "Root of the JSON file (no extension).", cxxopts::value<std::string>())
            ("p,pretty", "Prettify the output.", cxxopts::value<bool>()->default_value("false"))
            ("l,links", "Follow symbolic links.", cxxopts::value<bool>()->default_value("true"))
            ("r,recursive", "Look deeper even if no direct path is provided.", cxxopts::value<bool>()->default_value("true"))
            ("h,help", "Show some help.")
            ;

        auto result = options.parse(argc, argv);

        if (result.count("help")){
            std::cout << options.help() << std::endl;
            exit(0);
        }

        auto tree = parse_resource(result["file"].as<string>(), {result["base64"].as<bool>(),result["verbose"].as<bool>(),result["links"].as<bool>()});
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