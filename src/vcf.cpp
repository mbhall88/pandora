#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>
#include <ctime>
#include <vector>
#include "vcfrecord.h"
#include "vcf.h"
#include "utils.h"

using namespace std;

VCF::VCF() {};

VCF::~VCF() {
    clear();
};

void VCF::add_record(string c, uint32_t p, string r, string a, string i, string g)
{
    VCFRecord vr(c, p, r, a, i, g);
    if ( find(records.begin(), records.end(), vr) == records.end())
    {
	records.push_back(vr);
	records.back().samples.insert(records.back().samples.end(), samples.size(), ".");
    }
}

void VCF::add_record(VCFRecord& vr)
{
    if ( find(records.begin(), records.end(), vr) == records.end())
    {
        records.push_back(vr);
	records.back().samples.insert(records.back().samples.end(), samples.size(), ".");
    }
}

void VCF::add_sample_gt(std::string c, uint32_t p, std::string r, std::string a)
{
    string name = "sample";

    // if this sample has not been added before, add a column for it
    if (find(samples.begin(), samples.end(), name) == samples.end())
    {
	samples.push_back(name);
	for (uint i=0; i!=records.size(); ++i)
	{
	    records[i].samples.push_back(".");
	}
    }

    VCFRecord vr(c, p, r, a);        
    bool added = false;
    vector<VCFRecord>::iterator it = find(records.begin(), records.end(), vr);
    if (it != records.end())
    {
	it->samples[0] = "0/1";
    } else {
	// either we have the ref allele, or a mistake
	for (uint i=0; i!=records.size(); ++i)
	{
	    if (records[i].pos == p)
	    {
		assert(records[i].ref == r);
		records[i].samples[0] = "1/0";	
		added = true;
	    } else if (records[i].pos > p) {
		break;
	    }
	}
	assert(added == true);
    }

    return;
}

void VCF::clear()
{
    records.clear();
}

// NB in the absence of filter flags being set to true, all results are saved. If one or more filter flags for SVTYPE are set, 
// then only those matching the filter are saved. Similarly for GRAPHTYPE.
void VCF::save(const string& filepath, bool simple, bool complexgraph, bool snp, bool indel, bool phsnps, bool complexvar)
{
    cout << now() << "Saving VCF to " << filepath << endl;

    // find date
    time_t t = time(0);
    char mbstr[10];
    strftime(mbstr, sizeof(mbstr), "%d/%m/%y", localtime(&t));

    // open and write header
    ofstream handle;
    handle.open (filepath);

    handle << "##fileformat=VCFv4.3" << endl;
    handle << "##fileDate==" << mbstr << endl;
    handle << "##ALT=<ID=SNP,Description=\"SNP\">" << endl;
    handle << "##ALT=<ID=PH_SNPs,Description=\"Phased SNPs\">" << endl;
    handle << "##ALT=<ID=INDEL,Description=\"Insertion-deletion\">" << endl;
    handle << "##ALT=<ID=COMPLEX,Description=\"Complex variant, collection of SNPs and indels\">" << endl;
    handle << "##INFO=<ID=SVTYPE,Number=1,Type=String,Description=\"Type of variant\">" << endl;
    handle << "##ALT=<ID=SIMPLE,Description=\"Graph bubble is simple\">" << endl;
    handle << "##ALT=<ID=COMPLEX,Description=\"Variation site was a nested feature in the graph\">" << endl;
    handle << "##INFO=<ID=GRAPHTYPE,Number=1,Type=String,Description=\"Type of graph feature\">" << endl;
    handle << "#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\tFORMAT";
    for (uint i=0; i!=samples.size(); ++i)
    {
	handle << "\t" << samples[i];
    }
    handle << endl;

    sort(records.begin(), records.end()); // we need the records in order for it to be a valid vcf

    for (uint i=0; i!=records.size(); ++i)
    {
	if (((simple==false and complexgraph==false) or 
	     (simple==true and records[i].info.find("GRAPHTYPE=SIMPLE")!=std::string::npos) or
             (complexgraph==true and records[i].info.find("GRAPHTYPE=COMPLEX")!=std::string::npos)) and
	    ((snp==false and indel==false and phsnps==false and complexvar==false) or
	     (snp==true and records[i].info.find("SVTYPE=SNP")!=std::string::npos) or
             (indel==true and records[i].info.find("SVTYPE=INDEL")!=std::string::npos) or
             (phsnps==true and records[i].info.find("SVTYPE=PH_SNPs")!=std::string::npos) or
             (complexvar==true and records[i].info.find("SVTYPE=COMPLEX")!=std::string::npos)))
	{
            handle << records[i];
	}
    }
    handle.close();
    cout << now() << "Finished saving " << records.size() << " entries to file" << endl;
    return;
}

void VCF::load(const string& filepath)
{
    cout << now() << "Loading VCF from " << filepath << endl;
    VCFRecord vr;
    string line;
    stringstream ss;
    uint added = 0;
    // NB this doesn't currently clear records first. Do we want to?

    ifstream myfile (filepath);
    if (myfile.is_open())
    {
        while ( getline (myfile,line).good() )
        {
            if (line[0] != '#')
            {
		ss << line;
                ss >> vr;
		ss.clear();
		add_record(vr);
		added += 1;
	    }
	}
    } else {
        cerr << "Unable to open VCF file " << filepath << endl;
	exit(1);
    }
    cout << now() << "Finished loading " << added << " entries to VCF, which now has size " << records.size() << endl;
    return;
}

bool VCF::operator == (const VCF& y) const {
    if (records.size() != y.records.size()){return false;}
    for (uint i=0; i!=y.records.size(); ++i)
    {
	if ( find(records.begin(), records.end(), y.records[i] ) == records.end())
	{
	    return false;
	}
    }
    return true;
}
