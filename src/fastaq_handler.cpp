#include <string>
#include <cstring>
#include <iostream>
//#include <fstream>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include "fastaq_handler.h"
#include "utils.h"

using namespace std;

FastaqHandler::FastaqHandler(const string& filepath) : gzipped(false), instream(&inbuf), num_reads_parsed(0) {
    cout << now() << "Open fastaq file " << filepath << endl;
    fastaq_file.open(filepath);
    if (not fastaq_file.is_open()) {
        cerr << "Unable to open fastaq file " << filepath << endl;
        exit(EXIT_FAILURE);
    }
    try {
        if(filepath.substr( filepath.length() - 2 ) == "gz"){
            inbuf.push(boost::iostreams::gzip_decompressor());
            gzipped = true;
        }
        inbuf.push(fastaq_file);
    }
    catch(const boost::iostreams::gzip_error& e) {
        cerr<< "Problem transfering file contents to boost stream: " << e.what() << '\n';
    }
}

FastaqHandler::~FastaqHandler(){
    close();
}

bool FastaqHandler::eof()
{
    int c = instream.peek();
    return (c == EOF);
}

void FastaqHandler::get_next(){
    //cout << "next ";
    if (!line.empty() and (line[0] == '>' or line[0] == '@')) {
        //cout << "read name line " << num_reads_parsed << " " << line << endl;
        name = line.substr(1);
        ++num_reads_parsed;
        read.clear();
    }

    while (getline(instream, line).good()){
        if (!line.empty() and line[0] == '+') {
            //skip this line and the qual score line
            getline(instream, line);
            //cout << "qual score line ." << line << "." << endl;
        } else if (line.empty() || line[0] == '>' || line[0] == '@') {
            if (!read.empty() or line.empty()) // ok we'll allow reads with no name, removed
            {
                return;
            }
            //cout << num_reads_parsed << " " << line << endl;
            name = line.substr(1);
            ++num_reads_parsed;
            read.clear();
        } else {
            //cout << "read line ." << line << "." << endl;
            read += line;
        }
    }
}

void FastaqHandler::skip_next(){
    //cout << "skip ";
    if (!line.empty() and (line[0] == '>' or line[0] == '@')) {
        ++num_reads_parsed;
    }

    while (getline(instream, line).good()){
        if (!line.empty() and line[0] == '+') {
            //skip this line and the qual score line
            getline(instream, line);
            //cout << "qual score line ." << line << "." << endl;
        } else if (!line.empty() and (line[0] == '>' or line[0] == '@')) {
	        return;
	    }
    }
}

void FastaqHandler::get_id(const uint32_t& id){
    //cout << "get id " << id << endl;
    if (id < num_reads_parsed) {
        cout << "restart buffer as have id " << num_reads_parsed << " and want id " << id << endl;
        num_reads_parsed = 0;
        name.clear();
        read.clear();
        line.clear();
        assert(name.empty());
        assert(read.empty());
        assert(line.empty());
        fastaq_file.clear();
        fastaq_file.seekg(0, fastaq_file.beg);
        inbuf.pop();
        /*if(gzipped){
            inbuf.push(boost::iostreams::gzip_decompressor());
        }*/
        inbuf.push(fastaq_file);
        instream.clear();
        instream.sync();
    }

    while (id > 1 and num_reads_parsed < id) {
	    skip_next();
        if (eof())
            break;
    }

    while (num_reads_parsed <= id) {
        get_next();
        if (eof())
            break;
    }
    cout << "now have name " << name << " read " << read << " num_reads_parsed " << num_reads_parsed << " and line " << line << endl;
}

void FastaqHandler::close(){
    cout << now() << "Close fastaq file" << endl;
    fastaq_file.close();
}


