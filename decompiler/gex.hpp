/*
 *	gex.hpp
 *	GameMaker Extension
 */

#ifndef __GEX_HPP
#define __GEX_HPP

#include <iostream>
#include <gmk.hpp>
#include "obfuscation.hpp"

typedef struct _GexHeader {
	bool				editable, hidden;
	std::string			name, tempDirectory, version, author, dateModified, licence, information, helpFile;
	std::vector<std::string> uses;
} GexHeader;

typedef struct _GexFunction {
	bool				hidden;
	unsigned int		callConversion, resultType;
	std::string			name, externalName, helpLine;
	std::vector<int>	arguments;
} GexFunction;

typedef struct _GexConstant {
	bool				hidden;
	std::string			name, value;
} GexConstant;

typedef struct _GexIncludeFile {
	unsigned int		type;
	std::string			name, path, initCode, finalCode;
	std::vector<GexFunction> functions;
	std::vector<GexConstant> constants;
} GexIncludeFile;

class GmGex {
private:
	GmkStream* handle;
	GmObfuscation* obfuscation;
	
	int version;
	GexHeader header;
	std::vector<GexIncludeFile*> includeFiles;
	
public:
	GmGex() : handle(new GmkStream()), obfuscation(new GmObfuscation(0)) { }
	~GmGex() { delete obfuscation; }

	// Interface functions
	bool Load(const std::string& filename);
	bool Save(const std::string& filename);
	const unsigned int GetGexVersion() const { return version; }
};

#endif
