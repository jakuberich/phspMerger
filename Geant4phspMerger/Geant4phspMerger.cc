#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
#include <sys/stat.h>
#include "iaea_phsp.h"    // Operacje na plikach PHSP
#include "iaea_header.h"  // Obsługa nagłówków
#include "iaea_record.h"  // Operacje na rekordach
#include "utilities.h"    // Funkcje pomocnicze

using namespace std;

const int ERROR_THRESHOLD = 10; // Maksymalna liczba błędów odczytu dla jednego źródła

// Funkcja usuwająca istniejące pliki wyjściowe, jeśli są obecne
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
    
    // Ostatni argument to nazwa pliku wyjściowego, pozostałe to pliki wejściowe
    int numInput = argc - 2;
    vector<string> inputFiles;
    for (int i = 1; i <= numInput; i++) {
        inputFiles.push_back(argv[i]);
    }
    const char* outFile = argv[argc - 1];
    
    // Usuń istniejące pliki wyjściowe, aby mieć czysty start
    removeOutputFiles(outFile);
    
    // Zmienne globalne do scalania statystyk
    int globalExtraFloats = 0;
    int globalExtraLongs  = 0;
    IAEA_I64 mergedOrigHistories = 0;
    IAEA_I64 mergedTotalParticles  = 0;
    
    // Otworzymy pierwszy plik wejściowy – na jego podstawie skopiujemy nagłówek
    IAEA_I32 src1, src2, dest;
    IAEA_I32 res;
    int len1 = inputFiles[0].size();
    IAEA_I32 accessRead = 1;
    iaea_new_source(&src1, const_cast<char*>(inputFiles[0].c_str()), &accessRead, &res, len1);
    if (res < 0) {
        cerr << "Error opening input source: " << inputFiles[0] << endl;
        return 1;
    }
    // Pobieramy z pierwszego pliku liczby extra
    int exF, exL;
    iaea_get_extra_numbers(&src1, &exF, &exL);
    globalExtraFloats = exF;
    globalExtraLongs  = exL;
    // Pobieramy statystyki z pierwszego pliku
    IAEA_I64 origHist, totParticles;
    iaea_get_total_original_particles(&src1, &origHist);
    res = -1;
    iaea_get_max_particles(&src1, &res, &totParticles);
    mergedOrigHistories += origHist;
    mergedTotalParticles  += totParticles;
    
    // Tworzymy plik wyjściowy – używając nagłówka z pierwszego pliku
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
    
    // Przetwarzamy kolejne pliki wejściowe (od pierwszego do ostatniego)
    for (int i = 0; i < numInput; i++) {
        string currentFile = inputFiles[i];
        int currLen = currentFile.size();
        IAEA_I32 currSrc;
        if (i == 0) {
            currSrc = src1; // pierwszy plik już otwarty
        } else {
            iaea_new_source(&currSrc, const_cast<char*>(currentFile.c_str()), &accessRead, &res, currLen);
            if (res < 0) {
                cerr << "Error opening input source: " << currentFile << endl;
                continue; // pomijamy ten plik
            }
            // Aktualizujemy globalne maksimum dla extra
            int currExF = 0, currExL = 0;
            iaea_get_extra_numbers(&currSrc, &currExF, &currExL);
            if (currExF > globalExtraFloats)
                globalExtraFloats = currExF;
            if (currExL > globalExtraLongs)
                globalExtraLongs = currExL;
            // Sumujemy statystyki
            IAEA_I64 currOrig = 0, currTot = 0;
            iaea_get_total_original_particles(&currSrc, &currOrig);
            res = -1;
            iaea_get_max_particles(&currSrc, &res, &currTot);
            mergedOrigHistories += currOrig;
            mergedTotalParticles  += currTot;
        }
        
        // Pobieramy oczekiwaną liczbę rekordów z bieżącego pliku
        IAEA_I64 expected;
        res = -1;
        iaea_get_max_particles(&currSrc, &res, &expected);
        // Zakładamy, że nagłówek zawiera jeden rekord „za dużo”
        IAEA_I64 expectedRecords = (expected > 0) ? expected - 1 : expected;
        cout << "Processing source " << currentFile << " (expected records = " << expectedRecords << ")..." << endl;
        
        // Odczyt i zapis rekordów
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
            // Zamykamy bieżące źródło, jeśli nie jest to pierwszy plik
            iaea_destroy_source(&currSrc, &res);
        }
    }
    
    // Ustawiamy ujednoliconą sekcję extra w nagłówku wyjściowym
    iaea_set_extra_numbers(&dest, &globalExtraFloats, &globalExtraLongs);
    // Aktualizujemy statystyki nagłówka
    iaea_set_total_original_particles(&dest, &mergedOrigHistories);
    // Aktualizujemy liczbę cząstek – przyjmujemy sumę rekordów ze wszystkich plików
    iaea_update_header(&dest, &res);
    if (res < 0)
        cerr << "Error updating output header (code " << res << ")." << endl;
    else
        cout << "Output header updated successfully." << endl;
    
    // Diagnostyka: odczytujemy rozmiar pliku wynikowego na podstawie nazwy
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
    
    // Zamykamy plik wyjściowy oraz źródła wejściowe (dla pierwszego pliku już nie zamykamy, bo został użyty w pętli)
    iaea_destroy_source(&dest, &res);
    iaea_destroy_source(&src1, &res);
    // src2 został już użyty w pętli; jeśli nadal otwarty, zamykamy go
    iaea_destroy_source(&src2, &res);
    
    cout << "Merging complete." << endl;
    return 0;
}
