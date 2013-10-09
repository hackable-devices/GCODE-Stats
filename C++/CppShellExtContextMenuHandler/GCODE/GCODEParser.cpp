#include "../splash/Splash.h"
#include "GCODEParser.h"

#ifndef M_PI
   #define M_PI 3.14159265358979323846
#endif

float __filamentDiameter = 1.75f + 0.07f; // mm + bias (mm)
float __averageDensity = 1050.f; // kg.m-3
float __averageAccelerationEfficiencyWhenTravelling = 0.2f; // ratio : theoricalSpeed * averageAccelEfficiency = realSpeed along an average path
float __averageAccelerationEfficiencyWhenExtruding = 0.6f; // ratio : theoricalSpeed * averageAccelEfficiency = realSpeed along an average path

stats statistics;

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  static HINSTANCE hInstance;

  return DefWindowProc(hWnd, msg, wParam, lParam);
}

//Here we scan the float in the code
float scanFloat(std::string sourceString, std::string valueString) {
	std::string s(sourceString);

	size_t pos = s.find(valueString.c_str());
	if ( pos != string::npos ) {
		s.replace( pos, valueString.length(), "" );
	}

	float value = 0.f;
	value = atof(s.c_str());

	return value;
}

//Here we scan the integer in the code
int scanInt(std::string sourceString, std::string valueString) {
	std::string s(sourceString);
	size_t pos = s.find(valueString.c_str());
	if ( pos != string::npos ) {
		s.replace( pos, valueString.length(), "" );
	}

	int value = 0;
	value = atoi(s.c_str());

	return value;
}

//Here we scan the next string containing "valueString"
bool scanString(std::string sourceString, std::string valueString, std::string *intoString) {
	bool containing = false;

	if (sourceString.find(valueString) != std::string::npos) {
		containing = true;
		if (intoString != NULL) {
			intoString->operator=(sourceString);
		}
	}

	return containing;
}

//Here we update the location of our string stream
void scannerUpdateLocation(std::istringstream& iss, unsigned int location) {
	for (unsigned int i=0; i < location; i++) {
		std::string unused;
		iss >> unused;
	}
}

//Check for a regular-expression-like
bool scanCharactersFromSet(std::string sourceString, std::string charactersSet, std::string& intoString) {
	bool isTrue = true;

	for (unsigned int i=0; i < sourceString.length(); i++) {
		bool difference = true;
		for (unsigned int j=0; j < charactersSet.length(); j++) {
			if (sourceString[i] == charactersSet[j]) {
				difference = false;
				break;
			}
		}
		if (difference) {
			isTrue = false;
			break;
		}
	}

	if (isTrue) {
		intoString = sourceString;
	}

	return isTrue;
}

//Init the statistics
void initStatistics() {

    statistics.currentFeedRate = 4800.0; // Default feed rate (mm/min)
    
    statistics.currentLocation = vector3D(0,0,0);
    statistics.totalTravelledTime = 0;
    statistics.totalTravelledDistance = 0;
    statistics.totalExtrudedTime = 0;
    statistics.totalExtrudedDistance = 0;
    
    statistics.currentExtrudedLengthToolA = 0;
    statistics.currentExtrudedLengthToolB = 0;
    
    statistics.totalExtrudedLengthToolA = 0;
    statistics.totalExtrudedLengthToolB = 0;
    
    statistics.movementLinesCount = 0;
    statistics.layersCount = 0;
    statistics.layerHeight = 0;
    
    statistics.extruding = false;
    statistics.dualExtrusion = false;
    statistics.usingToolB = false;
}

//Here we update the stats
void updateStats(std::istringstream& iss, stats *GCODE_stats, vector3D *currentLocation) {

	vector3D previousLocation = GCODE_stats->currentLocation;

	GCODE_stats->currentLocation = *currentLocation;
    GCODE_stats->movementLinesCount++;

	std::string s;
	iss >> s;
	if (scanString(s, "F", NULL)) {
		GCODE_stats->currentFeedRate = scanFloat(s, "F");
	}

	float currentExtrudedLength = 0.f;

	iss >> s;
	if (scanString(s, "E", NULL) || scanString(s, "A", NULL)) {

		if (scanString(s, "E", NULL))
			currentExtrudedLength = scanFloat(s, "E");
		else
			currentExtrudedLength = scanFloat(s, "A");

		GCODE_stats->extruding = (currentExtrudedLength > GCODE_stats->currentExtrudedLengthToolA);
		if (GCODE_stats->extruding) {
			//Real life test
			GCODE_stats->currentExtrudedLengthToolA = currentExtrudedLength;
		}
		GCODE_stats->usingToolB = false;
	} else if (scanString(s, "B", NULL)) {
		currentExtrudedLength = scanFloat(s, "B");
		GCODE_stats->extruding = (currentExtrudedLength > GCODE_stats->currentExtrudedLengthToolB);
		if (GCODE_stats->extruding) {
			// Real life test
			GCODE_stats->currentExtrudedLengthToolB = currentExtrudedLength;
		}
		GCODE_stats->usingToolB = true;
	}

	vector3D* travelVector = new vector3D(GCODE_stats->currentLocation.x, GCODE_stats->currentLocation.y, GCODE_stats->currentLocation.z);
	travelVector->minus(previousLocation);
    float longestDistanceToMove = max(fabs(travelVector->x), fabs(travelVector->y)); // mm
	//printf("%f\n", longestDistanceToMove);
	float cartesianDistance = travelVector->getLength();

	// == Calculating time taken to move or extrude ==
    if (GCODE_stats->extruding){
        
        // Extrusion in progress, let's calculate the time taken
        // NSLog(@"Extruding %f  > %f", currentExtrudedLength, previousExtrudedLength);
        GCODE_stats->totalExtrudedDistance += cartesianDistance; // mm
        GCODE_stats->totalExtrudedTime += (longestDistanceToMove / (GCODE_stats->currentFeedRate *  __averageAccelerationEfficiencyWhenExtruding)); // min
    } else {
        
        // We're only travelling, let's calculate the time taken
        // NSLog(@"Travelling");
        GCODE_stats->totalTravelledDistance += cartesianDistance; // mm
        GCODE_stats->totalTravelledTime += (longestDistanceToMove / (GCODE_stats->currentFeedRate * __averageAccelerationEfficiencyWhenTravelling)); // min
    }
}

//Get the location in of the gcode X Y Z
void updateLocation(std::istringstream& iss, vector3D *currentLocation) {
	std::string s;
	float value;

	iss >> s;
	if (scanString(s, "X", NULL)) {
		value = scanFloat(s, "X");
		currentLocation->x = value;
		//printf("%f ", value);
	}
	iss >> s;
	if (scanString(s, "Y", NULL)) {
		value = scanFloat(s, "Y");
		currentLocation->y = value;
		//printf("%f ", value);
	}
	iss >> s;
	if (scanString(s, "Z", NULL)) {
		value = scanFloat(s, "Z");
		currentLocation->z = value;
		//printf("%f ", value);
	}
}

//If it's a new layer then we return true and we update the location
bool isNewLayer(std::istringstream &iss, vector3D *currentLoc) {

    bool isNewLayer = false;

	std::string s;
	iss >> s;

	if (s == "G1")
    {
		float oldZ = currentLoc->z;

		updateLocation(iss, currentLoc);

		bool layerChange = fabs(currentLoc->z - oldZ) > .09 && fabs(currentLoc->z - oldZ) < 100.0;
		// NSLog(@"%f", ABS(currentLocation.z - oldZ));
		//std::cout << abs(currentLoc.z - oldZ);

		if(layerChange) {
			// NSLog(@"New layer created at z = %f", currentLocation.z);
			//std::cout << "New Layer created at = " << currentLoc.z;
			isNewLayer = true;
		}
    }
  
    // Reset scan of line
    
    return isNewLayer;

}

//Here we parse the gocde
void parseGCode(HWND hWnd, HINSTANCE g_hInst, char *filePath) {

    std::string line;

	std::ifstream gcodeFile;
	gcodeFile.open(filePath);

	//Get number of lines in the file
	//int numLines = std::count(std::istreambuf_iterator<char>(gcodeFile), 
	//						  std::istreambuf_iterator<char>(), '\n');
	//wchar_t szMessage[300];
	//StringCchPrintf(szMessage, ARRAYSIZE(szMessage), L"%d lines", numLines);
	//MessageBox(hWnd, szMessage, L"Error", MB_OK);

	//Reset cursor file position
	//gcodeFile.seekg(0, ios::beg);

	CSplash splash1(TEXT(""), RGB(128, 128, 128), g_hInst);
	splash1.ShowSplash();

	if (hWnd) {
		if (gcodeFile.is_open()) {
			//MessageBox(hWnd, L"File Opened !", L"Error", MB_OK);
		}
	} else {
		printf("File Opened\n");
	}

	printf("Parsing GCode File\n");

	initStatistics();

	vector3D cornerLow(0.f, 0.f, 0.f);
	vector3D cornerHigh(0.f, 0.f, 0.f);
	float extrusionWidth = 0.f;

	vector3D currentLocation(0.f,0.f,0.f);
    vector3D highCorner(-FLT_MAX,-FLT_MAX,-FLT_MAX);
    vector3D lowCorner(FLT_MAX,FLT_MAX,FLT_MAX);

    while (getline(gcodeFile, line))
    {

        float oldZ = currentLocation.z;

		//std::istringstream *iss = new std::istringstream(line.c_str());
		std::istringstream iss(line.c_str());

        // Is this a new Layer ?
        if (isNewLayer(iss, &currentLocation)) {
            statistics.layersCount++;
            
            // If height has not been found yet
            if (statistics.layerHeight == 0.0){
                
                float theoreticalHeight = floor((currentLocation.z - oldZ)*100)/100;
                
                if (theoreticalHeight > 0 && theoreticalHeight < 1){ // We assume that a layer is less than 1mm thick
                    statistics.layerHeight = theoreticalHeight;
                }
            }
        } else {
			iss = std::istringstream(line.c_str());
		}

		std::string s;
		iss >> s;
		std::string command;
		bool commandFound = scanCharactersFromSet(s, "GMT0123456789", command);


		if (!commandFound) {
            continue;
        }

		if(command == "M104" || command == "M109" || command == "G10") {
			//printf("M104 M109 G10\n");
			// M104: Set Extruder Temperature
            // Set the temperature of the current extruder and return control to the host immediately
            // (i.e. before that temperature has been reached by the extruder). See also M109 that does the same but waits.
            // /!\ This is deprecated because temperatures should be set using the G10 and T commands.
                
            // G10
            // Example: G10 P3 X17.8 Y-19.3 Z0.0 R140 S205
            // This sets the offset for extrude head 3 (from the P3) to the X and Y values specified.
            // The R value is the standby temperature in oC that will be used for the tool, and the S value is its operating temperature.

            // Makerware puts the temperature first, skip it
			iss >> s;
			if (scanString(s, "S", NULL)) {
                scanInt(s, "S");
            }
			// Extract the tool index
			iss >> s;
            if (scanString(s, "P", NULL) || scanString(s, "T", NULL)) {
                int toolIndex;
				if (scanString(s, "P", NULL))
					toolIndex = scanInt(s, "P");
				else
					toolIndex = scanInt(s, "T");
                    
                bool previouslyUsingToolB = statistics.usingToolB;
                statistics.usingToolB = (toolIndex >= 1);
                    
                if (statistics.usingToolB == !previouslyUsingToolB) {
                    statistics.dualExtrusion = true;
                }
            }
		} else if(command == "G1") {
			//printf("G1\n");
			// Example: G1 X90.6 Y13.8 E22.4
            // Go in a straight line from the current (X, Y) point to the point (90.6, 13.8),
            // extruding material as the move happens from the current extruded length to a length of 22.4 mm.
			//iss = std::istringstream(line.c_str());

			updateLocation(iss, &currentLocation);

			lowCorner.minimizeWith(currentLocation);
            highCorner.maximizeWith(currentLocation);
                
            // Update stats
            updateStats(iss, &statistics, new vector3D(currentLocation));

		} else if(command == "G92") {
			//printf("G92\n");
			// G92: Set Position. Allows programming of absolute zero point, by reseting the current position
            // to the values specified.
            // Slic3r uses this to reset the extruded distance.
                
            // We assume that an E value appears first.
            // Generally, it's " G92 E0 ", but in case ...
			iss >> s;
			if (scanString(s, "E", NULL)) {
                float currentExtrudedLength;
                currentExtrudedLength = scanFloat(s, "E");
                if (statistics.usingToolB) {
                    statistics.totalExtrudedLengthToolB += statistics.currentExtrudedLengthToolB;
                    statistics.currentExtrudedLengthToolB = currentExtrudedLength;
                } else {
                    statistics.totalExtrudedLengthToolA += statistics.currentExtrudedLengthToolA;
                    statistics.currentExtrudedLengthToolA = currentExtrudedLength;
                }
            }
		} else if (command == "M135" || command == "M108") {
			//printf("M135 M108\n");
			// M135: tool switch.
            // M108: Set Extruder Speed.
            // Both are used in practice to swith the current extruder.
            // M135 is used by Makerware, M108 is used by Replicator G.
			iss >> s;
            if (scanString(s, "T", NULL)) {
                int toolIndex = scanInt(s, "T");
                    
                // BOOL previouslyUsingToolB = statistics.usingToolB;
                statistics.usingToolB = (toolIndex >= 1);
                    
                /*
                // The tool changed : we're sure we have a double extrusion print
                if (statistics.usingToolB == !previouslyUsingToolB) {
                    statistics.dualExtrusion = YES;
                }
                    */
            }
		} else if (command == "T0") {
			//printf("T0\n");
			// T0: Switch to the first extruder.
            // Slic3r and KISSlicer use this to switch the current extruder.
                
            // BOOL previouslyUsingToolB = statistics.usingToolB;
            statistics.usingToolB =  false;
                
            /*
            // The tool changed : we're sure we have a double extrusion print
            if (statistics.usingToolB == !previouslyUsingToolB) {
                statistics.dualExtrusion = YES;
            }
                */
		} else if (command == "T1") {
			//printf("T1\n");
            // T1: Switch to the second extruder.
            // Slic3r and KISSlicer use this to switch the current extruder.
                
            // BOOL previouslyUsingToolB = statistics.usingToolB;
            statistics.usingToolB =  true;
                
            /*
            // The tool changed : we're sure we have a double extrusion print
            if (statistics.usingToolB == !previouslyUsingToolB) {
                statistics.dualExtrusion = YES;
            }
                */
        }
    }

	statistics.totalExtrudedLengthToolA += statistics.currentExtrudedLengthToolA;
    statistics.totalExtrudedLengthToolB += statistics.currentExtrudedLengthToolB;

	// Correct extruded lengths for extruder primes
    statistics.totalExtrudedLengthToolA = statistics.dualExtrusion?statistics.totalExtrudedLengthToolA:(statistics.totalExtrudedLengthToolA>statistics.totalExtrudedLengthToolB?statistics.totalExtrudedLengthToolA:0);
    statistics.totalExtrudedLengthToolB = statistics.dualExtrusion?statistics.totalExtrudedLengthToolB:(statistics.totalExtrudedLengthToolB>statistics.totalExtrudedLengthToolA?statistics.totalExtrudedLengthToolB:0);

    // Correct height:
    highCorner.z = statistics.layersCount * statistics.layerHeight;

    cornerLow = lowCorner;
    cornerHigh = highCorner;
    extrusionWidth = statistics.layerHeight;

	printf("Finished Parsing GCode File\n\n");

    printf(" Total Extruded length Tool A (mm): %f\n", statistics.totalExtrudedLengthToolA);
    printf(" Total Extruded length Tool B (mm): %f\n", statistics.totalExtrudedLengthToolB);
    printf(" Using dual extrusion: %s\n", statistics.dualExtrusion ? "Yes" : "No");
    printf(" Grams : %f\n", (statistics.totalExtrudedLengthToolA + statistics.totalExtrudedLengthToolB) * M_PI/4 * pow(1.75,2) * 1050 * pow(10,-6));
        
    printf(" G1 Lines : %d\n",statistics.movementLinesCount );
        
    printf(" Layer Height : %f\n",statistics.layerHeight );
    printf(" Layer Count : %d\n",statistics.layersCount );
    printf(" Height Corrected (mm) : %f\n",statistics.layersCount*statistics.layerHeight );
        
    printf(" Total Extruded time (min): %f\n", statistics.totalExtrudedTime);
    printf(" Total Travelled time (min): %f\n", statistics.totalTravelledTime);
        
    printf(" Total Extruded distance (mm): %f\n", statistics.totalExtrudedDistance);
    printf(" Total Travelled distance (mm): %f\n", statistics.totalTravelledDistance);

	float raw = statistics.totalExtrudedTime + statistics.totalTravelledTime;
	float hours = floor(raw / 60);
    float minutes = floor(raw - hours * 60);

	printf("Total Time : ");

	if (hours > 0.f){
         std::cout << (int)hours << "h " << (int)minutes << "mins" << std::endl;
    } else {
        std::cout << (int)minutes << "mins" << std::endl;
    }

	splash1.CloseSplash();

	if (hWnd) {
		int intHours = INT(hours);
		int intMinutes = INT(minutes);

		wchar_t szMessage[3000];
		LPCTSTR pszFormat = TEXT("%s %d h %d min\n"
			L"Longueur de fil utilisée - Extrudeur A (cm): %.0f\n"
			L"Longueur de fil utilisée - Extrudeur B (cm): %.0f\n"
			L"Double Extrusion: %s\n"
			L"Poids (g) : %.0f\n"
			L"Hauteur moyenne des couches : %.0f microns\n"
			L"Nombre de couches : %d\n"
			L"Hauteur de l'objet (cm) : %.2f\n");
		TCHAR* pszTxt = TEXT("Temps Estimatif : ");
		if (SUCCEEDED(StringCchPrintf(szMessage, ARRAYSIZE(szMessage), 
			pszFormat,
			pszTxt, INT(hours), INT(minutes),
			statistics.totalExtrudedLengthToolA / 10.f,
			statistics.totalExtrudedLengthToolB / 10.f,
			statistics.usingToolB ? L"Oui" : L"Non",
			(statistics.totalExtrudedLengthToolA + statistics.totalExtrudedLengthToolB) * M_PI/4 * pow(1.75,2) * 1050.f * pow(10,-6),
			statistics.layerHeight * pow(10, 3),
			statistics.layersCount,
			(statistics.layersCount*statistics.layerHeight) / 10.f
			)))
		{
			MessageBox(hWnd, szMessage, L"CKAB GCODE Parser", MB_OK);
		}
	}
}

