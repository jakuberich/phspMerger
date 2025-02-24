#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
#include <sys/stat.h>
#include "iaea_phsp.h"    // PHSP file operations
#include "iaea_header.h"  // Header handling
#include "iaea_record.h"  // Record operations
#include "utilities.h"    // Helper functions

using namespace std;

const int ERROR_THRESHOLD = 10; // Maximum number of read errors allowed per source

// Function to remove existing output files, if present
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
    
    // The last argument is the output file base name, the others are input files
    int numInput = argc - 2;
    vector<string> inputFiles;
    for (int i = 1; i <= numInput; i++) {
        inputFiles.push_back(argv[i]);
    }
    const char* outFile = argv[argc - 1];
    
    // Remove any existing output files to ensure a clean start
    removeOutputFiles(outFile);
    
    // Global variables for merging statistics
    int globalExtraFloats = 0;
    int globalExtraLongs  = 0;
    IAEA_I64 mergedOrigHistories = 0;
    IAEA_I64 mergedTotalParticles  = 0;
    
    // Open the first input file and use its header as the base
    IAEA_I32 src1, src2, dest;
    IAEA_I32 res;
    int len1 = inputFiles[0].size();
    IAEA_I32 accessRead = 1;
    iaea_new_source(&src1, const_cast<char*>(inputFiles[0].c_str()), &accessRead, &res, len1);
    if (res < 0) {
        cerr << "Error opening input source: " << inputFiles[0] << endl;
        return 1;
    }
    // Get extra numbers from the first file
    int exF, exL;
    iaea_get_extra_numbers(&src1, &exF, &exL);
    globalExtraFloats = exF;
    globalExtraLongs  = exL;
    // Get statistics from the first file
    IAEA_I64 origHist, totParticles;
    iaea_get_total_original_particles(&src1, &origHist);
    res = -1;
    iaea_get_max_particles(&src1, &res, &totParticles);
    mergedOrigHistories += origHist;
    mergedTotalParticles  += totParticles;
    
    // Create the output file using the header from the first file
    int lenOut = strlen(outFile);
    IAEA_I32 accessWrite = 2;
    iaea_new_source(&dest, const_cast<char*>(outFile), &accessWrite, &res, lenOut);
    if (res < 0) {
        cerr << "Error creating output source: " << outFile << endl;
        iaea_destroy_source(&src1, &res);
        return 1;
    }
    iaea_copy_header(&src1, &dest, &res);
    if (res < 0) {
        cerr << "Error copying header from first input." << endl;
        iaea_destroy_source(&src1, &res);
        iaea_destroy_source(&dest, &res);
        return 1;
    }
    
    // Process the remaining input files (including the first one)
    for (int i = 0; i < numInput; i++) {
        string currentFile = inputFiles[i];
        int currLen = currentFile.size();
        IAEA_I32 currSrc;
        if (i == 0) {
            currSrc = src1; // first file already open
        } else {
            iaea_new_source(&currSrc, const_cast<char*>(currentFile.c_str()), &accessRead, &res, currLen);
            if (res < 0) {
                cerr << "Error opening input source: " << currentFile << endl;
                continue; // skip this file
            }
            // Update global maximum for extra values
            int currExF = 0, currExL = 0;
            iaea_get_extra_numbers(&currSrc, &currExF, &currExL);
            if (currExF > globalExtraFloats)
                globalExtraFloats = currExF;
            if (currExL > globalExtraLongs)
                globalExtraLongs = currExL;
            // Sum statistics
            IAEA_I64 currOrig = 0, currTot = 0;
            iaea_get_total_original_particles(&currSrc, &currOrig);
            res = -1;
            iaea_get_max_particles(&currSrc, &res, &currTot);
            mergedOrigHistories += currOrig;
            mergedTotalParticles  += currTot;
        }
        
        // Get the expected number of records from the current file
        IAEA_I64 expected;
        res = -1;
        iaea_get_max_particles(&currSrc, &res, &expected);
        // Assume the header has one extra record; read expected - 1 records
        IAEA_I64 expectedRecords = (expected > 0) ? expected - 1 : expected;
        cout << "Processing source " << currentFile << " (expected records = " << expectedRecords << ")..." << endl;
        
        // Read and write records
        IAEA_I64 count = 0;
        int errorCount = 0;
        IAEA_I32 n_stat, partType;
        IAEA_Float E, wt, x, y, z, u, v, w;
        float extraFloats[NUM_EXTRA_FLOAT];
        IAEA_I32 extraInts[NUM_EXTRA_LONG];
        for (IAEA_I64 j = 0; j < expectedRecords; j++) {
            iaea_get_particle(&currSrc, &n_stat, &partType, &E, &wt,
                              &x, &y, &z, &u, &v, &w,
                              extraFloats, extraInts);
            if (n_stat == -1) {
                errorCount++;
                cerr << "Error reading particle from " << currentFile << " at record " 
                     << j << " (error count: " << errorCount << ")" << endl;
                if (errorCount > ERROR_THRESHOLD) {
                    cerr << "Too many errors in " << currentFile << ". Aborting processing for this source." << endl;
                    break;
                }
                continue;
            }
            iaea_write_particle(&dest, &n_stat, &partType, &E, &wt,
                                &x, &y, &z, &u, &v, &w,
                                extraFloats, extraInts);
            count++;
            if (count % 1000000 == 0)
                cout << currentFile << ": Processed " << count << " records." << endl;
        }
        cout << currentFile << ": Total processed records: " << count << endl;
        if (i > 0) {
            // Close current source if it is not the first file
            iaea_destroy_source(&currSrc, &res);
        }
    }
    
    // Set unified extra section in the output header
    iaea_set_extra_numbers(&dest, &globalExtraFloats, &globalExtraLongs);
    // Update header statistics
    iaea_set_total_original_particles(&dest, &mergedOrigHistories);
    // Update particle count – using the sum of records from all input files
    iaea_update_header(&dest, &res);
    if (res < 0)
        cerr << "Error updating output header (code " << res << ")." << endl;
    else
        cout << "Output header updated successfully." << endl;
    
    // Diagnostic: read the output PHSP file size based on its name
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
    
    // Close the output source and input sources (src1 was used in the loop)
    iaea_destroy_source(&dest, &res);
    iaea_destroy_source(&src1, &res);
    // src2 has been used in the loop; if still open, close it
    iaea_destroy_source(&src2, &res);
    
    cout << "Merging complete." << endl;
    return 0;
}
