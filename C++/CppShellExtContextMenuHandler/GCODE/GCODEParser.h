

#ifndef __C_GCODE_PARSER_H_INCLUDED__
#define __C_GCODE_PARSER_H_INCLUDED__

#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <algorithm>

//#include <windows.h>

#include <shlobj.h>
#include <strsafe.h>

using namespace std;

struct vector3D {

    float x;
    float y;
    float z;

    vector3D(float _x=0, float _y=0, float _z=0):x(_x), y(_y), z(_z) { }
	vector3D(vector3D& v): x(v.x), y(v.y), z(v.z) { }

	float getLength() { return sqrt((x*x) + (y*y) + (z*z)); }

	void minus(vector3D v) {
		x -= v.x;
		y -= v.y;
		z -= v.z;
	}

	void minimizeWith(vector3D vector) {
		if (x > vector.x)
			x = vector.x;

		if (y > vector.y)
			y = vector.y;

		if (z > vector.z)
			z = vector.z;
	}

	void maximizeWith(vector3D vector) {
		if (x < vector.x)
			x = vector.x;

		if (y < vector.y)
			y = vector.y;

		if (z < vector.z)
			z = vector.z;
	}
};

struct stats {
    
    bool dualExtrusion;
    
	// Tool-specific stats
	/* TOOL A */
	float currentExtrudedLengthToolA;
	float totalExtrudedLengthToolA;
	/* TOOL B */
	float currentExtrudedLengthToolB;
	float totalExtrudedLengthToolB;
	bool usingToolB;
    
    // Common stats
    float totalExtrudedTime;
    float totalExtrudedDistance;
    
    float totalTravelledTime;
    float totalTravelledDistance;
    
    float currentFeedRate;
    
    int movementLinesCount;
    int layersCount;
    float layerHeight;
    
    vector3D currentLocation;
    
    bool extruding;
    
};

//typedef vector<vector<vector<int> > > vector3D;

void parseGCode(HWND hWnd, HINSTANCE g_hInst, char *filePath);
void updateStats(stats *GCODE_stats, vector3D *currentLocation);

#endif
