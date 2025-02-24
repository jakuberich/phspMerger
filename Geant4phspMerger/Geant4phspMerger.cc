#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
#include <sys/stat.h>
#include "iaea_phsp.h"    // Functions for PHSP file operations
#include "iaea_header.h"  // Header handling functions
#include "iaea_record.h"  // Record (particle) operations
#include "utilities.h"    // Helper functions

using namespace std;

const int ERROR_THRESHOLD = 10; // Maximum allowed read errors for one source

// Helper function: Remove existing output files (if any) so we start clean.
void removeOutputFiles(const char* baseName) {
    string headerFile = string(baseName) + ".IAEAheader";
    string phspFile   = string(baseName) + ".IAEAphsp";
    remove(headerFile.c_str());
    remove(phspFile.c_str());
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <inputFileBase1> [<inputFileBase2> ...] <outputFileBase>" << endl;
        return 1;
    }
    
    // The last argument is the output file base; all preceding ones are input files.
    int numInput = argc - 2;
    vector<string> inputFiles;
    for (int i = 1; i <= numInput; i++) {
        inputFiles.push_back(argv[i]);
    }
    const char* outFile = argv[argc - 1];
    
    // Remove any pre-existing output files for a clean start.
    removeOutputFiles(outFile);
    
    // Global counters for merged statistics.
    IAEA_I64 mergedOrigHistories = 0;
    IAEA_I64 mergedTotalParticles  = 0;
    
    // We store input source IDs in a vector so we can later destroy them.
    vector<IAEA_I32> inputSourceIDs;
    IAEA_I32 res;
    IAEA_I32 accessRead = 1;
    
    IAEA_I32 numExtraFloats = 0, numExtraInts = 0;
    
    for (size_t i = 0; i < inputFiles.size(); i++) {
        IAEA_I32 src;
        int len = inputFiles[i].size();
        iaea_new_source(&src, const_cast<char*>(inputFiles[i].c_str()), &accessRead, &res, len);
        if (res < 0) {
            cerr << "Error opening input source: " << inputFiles[i] << endl;
            continue;
        }
        inputSourceIDs.push_back(src);
        
        // Update merged statistics.
        IAEA_I64 origHist = 0, totParticles = 0;
        iaea_get_total_original_particles(&src, &origHist);
        res = -1;
        iaea_get_max_particles(&src, &res, &totParticles);
        mergedOrigHistories += origHist;
        mergedTotalParticles  += totParticles;

        IAEA_I32 tempExtraFloats, tempExtraInts;
        iaea_get_extra_numbers(&src, &tempExtraFloats, &tempExtraInts);
        numExtraFloats = max(numExtraFloats, tempExtraFloats);
        numExtraInts = max(numExtraInts, tempExtraInts);
    }
    
    if (inputSourceIDs.empty()) {
        cerr << "No valid input sources were opened. Aborting." << endl;
        return 1;
    }
    
    // Create output source using the output base file (extension .IAEAheader will be added)
    IAEA_I32 dest;
    int lenOut = strlen(outFile);
    IAEA_I32 accessWrite = 2; // writing mode
    iaea_new_source(&dest, const_cast<char*>(outFile), &accessWrite, &res, lenOut);
    if (res < 0) {
        cerr << "Error creating output source: " << outFile << endl;
        // Clean up all input sources.
        for (size_t i = 0; i < inputSourceIDs.size(); i++) {
            iaea_destroy_source(&inputSourceIDs[i], &res);
        }
        return 1;
    }
    
    iaea_copy_header(&inputSourceIDs[0], &dest, &res);
    iaea_set_extra_numbers(&dest, &numExtraFloats, &numExtraInts);
    
    IAEA_I32 extraLongTypes[NUM_EXTRA_LONG], extraFloatTypes[NUM_EXTRA_FLOAT];
    IAEA_I32 result;
    iaea_get_type_extra_variables(&inputSourceIDs[0], &result, extraLongTypes, extraFloatTypes);
    
    for (IAEA_I32 i = 0; i < numExtraInts; i++) {
        iaea_set_type_extralong_variable(&dest, &i, &extraLongTypes[i]);
    }
    
    for (IAEA_I32 i = 0; i < numExtraFloats; i++) {
        iaea_set_type_extrafloat_variable(&dest, &i, &extraFloatTypes[i]);
    }
    
    
    for (size_t idx = 0; idx < inputSourceIDs.size(); idx++) {
        IAEA_I32 currSrc = inputSourceIDs[idx];
        
        // Get the expected number of records from the header.
        IAEA_I64 expected;
        res = -1;
        iaea_get_max_particles(&currSrc, &res, &expected);
        // Assume the header contains one extra record, so we process expected - 1 records.
        IAEA_I64 expectedRecords = (expected > 0) ? expected - 1 : expected;
        cout << "Processing source " << inputFiles[idx] << " (expected records = " << expectedRecords << ")..." << endl;
        
        IAEA_I64 count = 0;
        int errorCount = 0;
        IAEA_I32 n_stat, partType;
        IAEA_Float E, wt, x, y, z, u, v, w;
        // In this merger we pass extra data through unchanged.
        float extraFloats[NUM_EXTRA_FLOAT];
        IAEA_I32 extraInts[NUM_EXTRA_LONG];
        
        for (IAEA_I64 j = 0; j < expectedRecords; j++) {
            iaea_get_particle(&currSrc, &n_stat, &partType, &E, &wt,
                              &x, &y, &z, &u, &v, &w,
                              extraFloats, extraInts);
            if (n_stat == -1) {
                errorCount++;
                cerr << "Error reading particle from " << inputFiles[idx] << " at record " 
                     << j << " (error count: " << errorCount << ")" << endl;
                if (errorCount > ERROR_THRESHOLD) {
                    cerr << "Too many errors in " << inputFiles[idx] << ". Aborting processing for this source." << endl;
                    break;
                }
                continue;
            }
            iaea_write_particle(&dest, &n_stat, &partType, &E, &wt,
                                &x, &y, &z, &u, &v, &w,
                                extraFloats, extraInts);
            count++;
            if (count % 1000000 == 0)
                cout << inputFiles[idx] << ": Processed " << count << " records." << endl;
        }
        cout << inputFiles[idx] << ": Total processed records: " << count << endl;
    }
    
    // Update the output header with merged statistics.
    iaea_set_total_original_particles(&dest, &mergedOrigHistories);
    iaea_update_header(&dest, &res);
    if (res < 0)
        cerr << "Error updating output header (code " << res << ")." << endl;
    else
        cout << "Output header updated successfully." << endl;
    
    // Diagnostic: Read and print the output PHSP file size.
    string mergedPhspPath = string(outFile) + ".IAEAphsp";
    struct stat fileStatus;
    FILE* fp = fopen(mergedPhspPath.c_str(), "rb");
    if (fp) {
        if (fstat(fileno(fp), &fileStatus) == 0) {
            IAEA_I64 fileSize = fileStatus.st_size;
            cout << "Output PHSP file size: " << fileSize << " bytes." << endl;
        }
        fclose(fp);
    } else {
        cerr << "Cannot open output PHSP file for size check: " << mergedPhspPath << endl;
    }
    
    // Destroy all sources.
    // First, destroy each input source.
    for (size_t i = 0; i < inputSourceIDs.size(); i++) {
        iaea_destroy_source(&inputSourceIDs[i], &res);
    }
    // Then, destroy the output source.
    iaea_destroy_source(&dest, &res);
    
    cout << "Merging complete." << endl;
    return 0;
}
