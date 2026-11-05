#pragma once
#include "../support/Arduino.h"
#include <algorithm>
#include <type_traits>
#include <sstream>
#include <iomanip>
#include <ctype.h>
#include <string.h>
#define PROGMEM
class String {
    std::string value;
public:
    String(){} String(const char* s):value(s?s:""){} String(const std::string& s):value(s){}
    template<class T,typename std::enable_if<std::is_integral<T>::value,int>::type=0>String(T x):value(std::to_string(x)){}
    String(double x,unsigned precision){std::ostringstream s;s<<std::fixed<<std::setprecision(precision)<<x;value=s.str();}
    size_t length()const{return value.size();}const char* c_str()const{return value.c_str();}
    void reserve(size_t n){value.reserve(n);}
    char operator[](size_t i)const{return value[i];}
    String& operator+=(const String& x){value+=x.value;return *this;}
    String& operator+=(char c){value+=c;return *this;}
    friend String operator+(const String&a,const String&b){return a.value+b.value;}
    friend String operator+(const String&a,char b){return a.value+b;}
    bool operator==(const String& b)const{return value==b.value;}
    bool operator!=(const String& b)const{return value!=b.value;}
    int indexOf(const String& text,size_t at=0)const{auto n=value.find(text.value,at);return n==std::string::npos?-1:int(n);}
    int indexOf(char c,size_t at=0)const{auto n=value.find(c,at);return n==std::string::npos?-1:int(n);}
    String substring(size_t a,size_t b=std::string::npos)const{a=std::min(a,value.size());b=std::min(b,value.size());if(b<a)std::swap(a,b);return value.substr(a,b-a);}
    void trim(){const auto a=value.find_first_not_of(" \r\n\t"),b=value.find_last_not_of(" \r\n\t");value=a==std::string::npos?"":value.substr(a,b-a+1);}
    void toLowerCase(){for(char& c:value)c=tolower(static_cast<unsigned char>(c));}
    void replace(const String& a,const String& b){size_t at=0;while((at=value.find(a.value,at))!=std::string::npos){value.replace(at,a.length(),b.value);at+=b.length();}}
    bool endsWith(const String& s)const{return value.size()>=s.length()&&value.compare(value.size()-s.length(),s.length(),s.value)==0;}
};
