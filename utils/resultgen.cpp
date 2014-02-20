/*
Copyright 2013 Henrik Mühe and Florian Funke

This file is part of CampersCoreBurner.

CampersCoreBurner is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

CampersCoreBurner is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with CampersCoreBurner.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "../include/core.h"
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <cassert>
#include <iostream>
using namespace std;

///////////////////////////////////////////////////////////////////////////////////////////////

char temp[MAX_DOC_LENGTH];

void TestSigmod(const char* test_file_str, const char* test_file_str_out)
{
	ofstream out(test_file_str_out);
    auto& newfile=out;
	FILE* test_file=fopen(test_file_str, "rt");

	if(!test_file)
	{
		printf("Cannot Open File %s\n", test_file_str);
		fflush(NULL);
		return;
	}

	InitializeIndex();

	while(1)
	{
		char ch;
		unsigned int id;

		if(EOF==fscanf(test_file, "%c %u ", &ch, &id))
			break;

		if(ch=='s')
		{
			int match_type;
			int match_dist;
            int count;

			if(EOF==fscanf(test_file, "%d %d %d %[^\n\r] ", &match_type, &match_dist, &count, temp))
			{
				printf("Corrupted Test File.\n");
				fflush(NULL);
				return;
			}
			
			ErrorCode err=StartQuery(id, temp, (MatchType)match_type, match_dist);

			if(err==EC_FAIL)
			{
				printf("The call to StartQuery() returned EC_FAIL.\n");
				fflush(NULL);
				return;
			}
			else if(err!=EC_SUCCESS)
			{
				printf("The call to StartQuery() returned unknown error code.\n");
				fflush(NULL);
				return;
			}
            
            newfile << "s " << id << " " << match_type << " " << match_dist << " " << count << " " << temp << endl;
		}
		else if(ch=='e')
		{
			ErrorCode err=EndQuery(id);

			if(err==EC_FAIL)
			{
				printf("The call to EndQuery() returned EC_FAIL.\n");
				fflush(NULL);
				return;
			}
			else if(err!=EC_SUCCESS)
			{
				printf("The call to EndQuery() returned unknown error code.\n");
				fflush(NULL);
				return;
			}
            newfile << "e " << id << endl;
		}
		else if(ch=='m')
		{
            unsigned count;
			if(EOF==fscanf(test_file, "%u %[^\n\r] ", &count, temp))
			{
				printf("Corrupted Test File.\n");
				fflush(NULL);
				return;
			}

			ErrorCode err=MatchDocument(id, temp);

			if(err==EC_FAIL)
			{
				printf("The call to MatchDocument() returned EC_FAIL.\n");
				fflush(NULL);
				return;
			}
			else if(err!=EC_SUCCESS)
			{
				printf("The call to MatchDocument() returned unknown error code.\n");
				fflush(NULL);
				return;
			}
            
            newfile << "m " << id << " " << count << " " << temp << endl;
            
			unsigned int doc_id=0;
			unsigned int num_res=0;
			unsigned int* query_ids=0;

			err=GetNextAvailRes(&doc_id, &num_res, &query_ids);
            assert(err==EC_SUCCESS);
            assert(doc_id==id);

            newfile << "r " << doc_id << " " << num_res << " ";
            bool firstDone=false;
            for (uint64_t index=0;index<num_res;++index)
                if (!firstDone) {
                    newfile << query_ids[index]; firstDone=true;
                } else {
                    newfile << " " << query_ids[index];
                }
            newfile << endl;

			if(num_res && query_ids) free(query_ids);
		}
		else if(ch=='r')
		{
            assert(false&&"this is not supposed to be used on a file containing R records.");
		}
		else
		{
			printf("Corrupted Test File. Unknown Command %c.\n", ch);
			fflush(NULL);
			return;
		}
	}

	DestroyIndex();

	fclose(test_file);
}

///////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
	if(argc<=2) {
		cout << "./resultgen input output" << endl;
		return -1;
	}
	TestSigmod(argv[1], argv[2]);
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////
