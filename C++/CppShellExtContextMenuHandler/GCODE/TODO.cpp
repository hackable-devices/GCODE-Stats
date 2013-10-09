
// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv CODE XCODE vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv CODE XCODE vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv CODE XCODE vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv CODE XCODE vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv CODE XCODE vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv CODE XCODE vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

- (void)updateStats:(struct stats*)GCODE_stats with:(Vector3*)currentLocation
{    
    // Travelling
    Vector3* previousLocation = GCODE_stats->currentLocation;
    GCODE_stats->currentLocation = [[currentLocation copy] autorelease];
    GCODE_stats->movementLinesCount++;
            
    // == Look for a feedrate FIRST ==
    if([self scanString:@"F" intoString:nil])
  {
    [self scanFloat:&GCODE_stats->currentFeedRate]; // mm/min
    }
    
    // == Look for an extrusion length ==
    // E or A is the first extruder head ("ToolA")
    // B is the other extruder ("ToolB")
    float currentExtrudedLength;
    
    if([self scanString:@"E" intoString:nil] || [self scanString:@"A" intoString:nil]) {
        
        // We're using ToolA for this move
        [self scanFloat:&currentExtrudedLength];
        GCODE_stats->extruding = (currentExtrudedLength > GCODE_stats->currentExtrudedLengthToolA);
        if (GCODE_stats->extruding) {
            // Real life test
            GCODE_stats->currentExtrudedLengthToolA = currentExtrudedLength;
        }
        GCODE_stats->usingToolB = NO;
        
  } else if ([self scanString:@"B" intoString:nil]) {
        
        // We're using ToolB for this move
        [self scanFloat:&currentExtrudedLength];
        GCODE_stats->extruding = (currentExtrudedLength > GCODE_stats->currentExtrudedLengthToolB);
        if (GCODE_stats->extruding) {
            // Real life test
            GCODE_stats->currentExtrudedLengthToolB = currentExtrudedLength;
        }
        GCODE_stats->usingToolB = YES;
    }
    
    //NSLog(@" ## Previous : %@", [previousLocation description]);
    //NSLog(@" ## Current : %@", [GCODE_stats->currentLocation description]);
    
    Vector3* travelVector = [GCODE_stats->currentLocation sub:previousLocation];
    float longestDistanceToMove = MAX(ABS(travelVector.x), ABS(travelVector.y)); // mm
    float cartesianDistance = [travelVector abs]; // mm
    
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
    
    // NSLog(@" ## tel= %f; tet= %f; ttt=%f; D=%f; fr=%f; extr=%d", GCODE_stats->currentExtrudedLengthToolA, GCODE_stats->totalExtrudedTime, GCODE_stats->totalTravelledTime, longestDistanceToMove, GCODE_stats->currentFeedRate, GCODE_stats->extruding);

    [self setScanLocation:0];
    
}

@end



- (id)initWithGCodeString:(NSString*)gcode;
{

    // Scan each line.
    [untrimmedLines enumerateObjectsUsingBlock:^(id untrimmedLine, NSUInteger idx, BOOL *stop) {
            NSScanner* lineScanner = [NSScanner scannerWithString:[untrimmedLine stringByTrimmingCharactersInSet:whiteSpaceSet]];
            
            float oldZ = currentLocation.z;
            
            if ([lineScanner isNewLayerWithCurrentLocation:currentLocation]){
                
                currentPane = [NSMutableArray array];
                [panes addObject:currentPane];
                statistics.layersCount++;
                
                // If height has not been found yet
                if (statistics.layerHeight == 0.0){
                    
                    float theoreticalHeight = roundf((currentLocation.z - oldZ)*100)/100;
                    
                    if (theoreticalHeight > 0 && theoreticalHeight < 1){ // We assume that a layer is less than 1mm thick
                        statistics.layerHeight = theoreticalHeight;
                    }
                    
                }
                
            }
            
            // Look for GCode commands starting with G, M or T.
            NSCharacterSet* commandCharacterSet = [NSCharacterSet characterSetWithCharactersInString:@"GMT0123456789"];
            NSString* command = nil;
            BOOL commandFound = [lineScanner scanCharactersFromSet:commandCharacterSet intoString:&command];

            if (!commandFound) {
                return;
            }

            if([command isEqualToString:@"M104"] || [command isEqualToString:@"M109"] || [command isEqualToString:@"G10"]) {
                // M104: Set Extruder Temperature
                // Set the temperature of the current extruder and return control to the host immediately
                // (i.e. before that temperature has been reached by the extruder). See also M109 that does the same but waits.
                // /!\ This is deprecated because temperatures should be set using the G10 and T commands.
                
                // G10
                // Example: G10 P3 X17.8 Y-19.3 Z0.0 R140 S205
                // This sets the offset for extrude head 3 (from the P3) to the X and Y values specified.
                // The R value is the standby temperature in oC that will be used for the tool, and the S value is its operating temperature.

                // Makerware puts the temperature first, skip it
                if ([lineScanner scanString:@"S" intoString:nil]) {
                    [lineScanner scanInt:nil];
                }
                
                // Extract the tool index
                if ([lineScanner scanString:@"P" intoString:nil] || [lineScanner scanString:@"T" intoString:nil]) {
                    
                    int toolIndex;
                    [lineScanner scanInt:&toolIndex];
                    
                    BOOL previouslyUsingToolB = statistics.usingToolB;
                    statistics.usingToolB = (toolIndex >= 1);
                    
                    if (statistics.usingToolB == !previouslyUsingToolB) {
                        statistics.dualExtrusion = YES;
                    }
                }
                
                // Done : We don't need the temperature
                
            } else if([command isEqualToString:@"G1"]) {
                // Example: G1 X90.6 Y13.8 E22.4
                // Go in a straight line from the current (X, Y) point to the point (90.6, 13.8),
                // extruding material as the move happens from the current extruded length to a length of 22.4 mm.
                
                [lineScanner updateLocation:currentLocation];
                                 
                [lowCorner minimizeWith:currentLocation];
                [highCorner maximizeWith:currentLocation];
                
                // Update stats
                [lineScanner updateStats:&statistics with:currentLocation];
                
                // Coloring
                if(statistics.extruding)
                {
                    if (statistics.dualExtrusion) {
                        if (statistics.usingToolB) {
                            [currentPane addObject:[_extrusionColors_B objectAtIndex:extrusionNumber%[_extrusionColors_B count]]];
                        } else {
                            [currentPane addObject:[_extrusionColors_A objectAtIndex:extrusionNumber%[_extrusionColors_A count]]];
                        }
                    } else {
                        [currentPane addObject:[_extrusionColors objectAtIndex:extrusionNumber%[_extrusionColors count]]];
                    }
                    
                } else {
                    extrusionNumber++;
                    [currentPane addObject:_extrusionOffColor];
                }
                
                [currentPane addObject:[[currentLocation copy] autorelease]];
            
            } else if([command isEqualToString:@"G92"]) {
                // G92: Set Position. Allows programming of absolute zero point, by reseting the current position
                // to the values specified.
                // Slic3r uses this to reset the extruded distance.
                
                // We assume that an E value appears first.
                // Generally, it's " G92 E0 ", but in case ...
                if ([lineScanner scanString:@"E" intoString:nil]) {
                    float currentExtrudedLength;
                    [lineScanner scanFloat:&currentExtrudedLength];
                    if (statistics.usingToolB) {
                        statistics.totalExtrudedLengthToolB += statistics.currentExtrudedLengthToolB;
                        statistics.currentExtrudedLengthToolB = currentExtrudedLength;
                    } else {
                        statistics.totalExtrudedLengthToolA += statistics.currentExtrudedLengthToolA;
                        statistics.currentExtrudedLengthToolA = currentExtrudedLength;
                    }
                }
                
                
            } else if ([command isEqualToString:@"M135"] || [command isEqualToString:@"M108"]) {
                // M135: tool switch.
                // M108: Set Extruder Speed.
                // Both are used in practice to swith the current extruder.
                // M135 is used by Makerware, M108 is used by Replicator G.
                if ([lineScanner scanString:@"T" intoString:nil]) {
                    int toolIndex;
                    [lineScanner scanInt:&toolIndex];
                    
                    // BOOL previouslyUsingToolB = statistics.usingToolB;
                    statistics.usingToolB = (toolIndex >= 1);
                    
                    /*
                    // The tool changed : we're sure we have a double extrusion print
                    if (statistics.usingToolB == !previouslyUsingToolB) {
                        statistics.dualExtrusion = YES;
                    }
                     */
                }
            } else if ([command isEqualToString:@"T0"]) {
                // T0: Switch to the first extruder.
                // Slic3r and KISSlicer use this to switch the current extruder.
                
                // BOOL previouslyUsingToolB = statistics.usingToolB;
                statistics.usingToolB =  NO;
                
                /*
                // The tool changed : we're sure we have a double extrusion print
                if (statistics.usingToolB == !previouslyUsingToolB) {
                    statistics.dualExtrusion = YES;
                }
                 */
                
            } else if ([command isEqualToString:@"T1"]) {
                // T1: Switch to the second extruder.
                // Slic3r and KISSlicer use this to switch the current extruder.
                
                // BOOL previouslyUsingToolB = statistics.usingToolB;
                statistics.usingToolB =  YES;
                
                /*
                // The tool changed : we're sure we have a double extrusion print
                if (statistics.usingToolB == !previouslyUsingToolB) {
                    statistics.dualExtrusion = YES;
                }
                 */
            }
            
    }];
        
        statistics.totalExtrudedLengthToolA += statistics.currentExtrudedLengthToolA;
        statistics.totalExtrudedLengthToolB += statistics.currentExtrudedLengthToolB;
        
        // Correct height:
        highCorner.z = statistics.layersCount * statistics.layerHeight;
        
    cornerLow = lowCorner;
    cornerHigh = highCorner;
    extrusionWidth = statistics.layerHeight;

    
    /*
    NSLog(@" High corner: %@", cornerHigh);
    NSLog(@" Low corner: %@", cornerLow);
    NSLog(@" Total Extruded length Tool A (mm): %f", statistics.totalExtrudedLengthToolA);
    NSLog(@" Total Extruded length Tool B (mm): %f", statistics.totalExtrudedLengthToolB);
    NSLog(@" Using dual extrusion: %@", statistics.dualExtrusion ? @"Yes" : @"No");
    NSLog(@" Grams : %f", (statistics.totalExtrudedLengthToolA + statistics.totalExtrudedLengthToolB) * pi/4 * pow(1.75,2) * 1050 * pow(10,-6));
    
    NSLog(@" G1 Lines : %d",statistics.movementLinesCount );
    
    NSLog(@" Layer Height : %f",statistics.layerHeight );
    NSLog(@" Layer Count : %d",statistics.layersCount );
    NSLog(@" Height Corrected (mm) : %f",statistics.layersCount*statistics.layerHeight );
    
    NSLog(@" Total Extruded time (min): %f", statistics.totalExtrudedTime);
    NSLog(@" Total Travelled time (min): %f", statistics.totalTravelledTime);
    
    NSLog(@" Total Extruded distance (mm): %f", statistics.totalExtrudedDistance);
    NSLog(@" Total Travelled distance (mm): %f", statistics.totalTravelledDistance);
    // */
