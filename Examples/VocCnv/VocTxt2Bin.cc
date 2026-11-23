/**
* This file is part of ORB-SLAM2.
*
* Copyright (C) 2014-2016 Raúl Mur-Artal <raulmur at unizar dot es> (University of Zaragoza)
* For more information see <https://github.com/raulmur/ORB_SLAM2>
*
* ORB-SLAM2 is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* ORB-SLAM2 is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with ORB-SLAM2. If not, see <http://www.gnu.org/licenses/>.
*/

#include <unistd.h>

#include<iostream>
#include "ORBVocabulary.h"
//#include "System.h"

using namespace ORB_SLAM2;

int main(int argc, char **argv)
{
  if (argc != 3)
    {
      cerr << endl << "Usage: VocTxt2Bin path_to_text_vocabulary path_to_bin_vocabulary" << endl;
      return 1;
    }

  string strTxtVocFile(argv[1]);
  string strBinVocFile(argv[2]);
    
  // Load ORB Vocabulary
  
  cout << endl << "Loading ORB Vocabulary. This could take a while..." << endl;
  ORB_SLAM2::ORBVocabulary *mpVocabulary = new ORBVocabulary();
  
  bool bVocLoad = false;
  if(strTxtVocFile.substr( strTxtVocFile.length() - 4) == ".txt")
    {
      cout << "Attempting load" << endl;
      bVocLoad = mpVocabulary->loadFromTextFile(strTxtVocFile);
    }
  
  if(!bVocLoad)
    {
      cerr << "Wrong path to vocabulary. " << endl;
      cerr << "Failed to open at: " << strTxtVocFile << endl;
      exit(-1);
    }
  cout << "Text vocabulary loaded!" << endl << endl;
  
  mpVocabulary->saveToBinaryFile(strBinVocFile);
  
  cout << "Binary vocabulary saved!" << endl << endl;

  return 0;
}


