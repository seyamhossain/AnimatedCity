#include <GL/glut.h>
#include <cmath>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <algorithm> // For std::min/max
#include <limits>    // For numeric_limits
#include <string>    // For std::to_string
#include <sstream>   // For string formatting
#include <iomanip>   // For setprecision

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- Configuration ---
bool ENABLE_DAY_NIGHT_CYCLE = true;
const int NUM_CARS = 8;
const int NUM_SIDEWALK_PEDESTRIANS = 10;
const int NUM_CROSSING_PEDESTRIANS = 6;
const int NUM_TREES = 12;
const int NUM_STREETLIGHTS = 6;
const int NUM_CLOUDS = 5;
const float PEDESTRIAN_WAIT_X_OFFSET = 15.0f;
const float CAR_MIN_SAFE_DISTANCE = 25.0f;
const float CAR_DECELERATION = 0.08f;
const float CAR_ACCELERATION = 0.04f;
const float STOP_LINE_DISTANCE_BEFORE_CROSSING = 15.0f;
const float CAR_TIME_PREDICTION_FACTOR = 1.15f;
const float NIGHT_START_TIME = 0.65f; // Adjust slightly for more overlap with sunset
const float NIGHT_END_TIME = 0.18f;
const float DAWN_DURATION = 0.1f; // Duration of dawn/dusk transition for clouds/lights
const float DUSK_DURATION = 0.1f;

// --- Global Variables ---
int windowWidth = 1000;
int windowHeight = 600;
float timeOfDay = 0.15f;
float timeSpeed = ENABLE_DAY_NIGHT_CYCLE ? 0.0001f : 0.0f;
enum LightState { RED, YELLOW, GREEN };
LightState trafficLightState = GREEN;
int trafficLightTimer = 0;
const int RED_DURATION = 250;
const int YELLOW_DURATION = 50;
const int GREEN_DURATION = 500;
float trafficLightX = windowWidth * 0.4f;
float zebraCrossingX = trafficLightX + 15;
float zebraCrossingWidth = 40.0f;
float crossingFrontEdge = zebraCrossingX - zebraCrossingWidth / 2.0f;
float crossingBackEdge = zebraCrossingX + zebraCrossingWidth / 2.0f;
float stopLineLeft = crossingFrontEdge - STOP_LINE_DISTANCE_BEFORE_CROSSING;
float stopLineRight = crossingBackEdge + STOP_LINE_DISTANCE_BEFORE_CROSSING;
float roadTopY = windowHeight * 0.30f;
float roadBottomY = windowHeight * 0.15f;
float footpathHeight = 30.0f;
float upperFootpathBottomY = roadTopY;
float upperFootpathTopY = upperFootpathBottomY + footpathHeight;
float lowerFootpathTopY = roadBottomY;
float lowerFootpathBottomY = 0;
float upperSidewalkLevelY = upperFootpathBottomY + footpathHeight * 0.6f;
float lowerSidewalkLevelY = lowerFootpathBottomY + footpathHeight * 0.4f;
float crossingStartY = upperFootpathBottomY - 2;
float crossingEndY = lowerFootpathTopY + 2;
float crossingWalkX = zebraCrossingX;
const float LANE_Y1 = roadBottomY + (roadTopY - roadBottomY) * 0.3f;
const float LANE_Y2 = roadBottomY + (roadTopY - roadBottomY) * 0.7f;

// --- Structures ---
struct Point {
	float x, y;
};
struct Color {
	float r, g, b;
};
struct Bird {
	float x,y,speed,flapPhase,flapSpeed,bobPhase;
};
std::vector<Bird> birds;
float birdBaseY = windowHeight*0.8f;
float birdAmplitudeY = 15.0f;
enum VehicleType { CAR, BUS, TRUCK };
struct Vehicle {
	float x,y,speed,baseSpeed,width,height;
	Color color;
	VehicleType type;
	int direction;
};
std::vector<Vehicle> vehicles;
enum PedestrianState { WALKING_SIDEWALK, WAITING_TO_CROSS, CROSSING, FINISHED_CROSSING };
struct Pedestrian {
	float x,y,speed,targetY,legPhase,legSpeed;
	PedestrianState state;
	Color clothingColor;
	bool onUpperPath;
};
std::vector<Pedestrian> sidewalkPedestrians;
std::vector<Pedestrian> crossingPedestrians;
struct Tree {
	Point pos;
	float scale;
	Color foliageColor;
	Color trunkColor;
};
std::vector<Tree> trees;
struct StreetLight {
	Point pos;
	float height;
	float armLength;
	bool onUpper;
};
std::vector<StreetLight> streetLights;
struct Cloud {
	Point pos;
	float speed;
	float scale;
	int numEllipses;
	std::vector<Point> ellipseOffsets;
	std::vector<float> ellipseRadiiX;
	std::vector<float> ellipseRadiiY;
	float shapePhase;
	float alpha;
}; // Added alpha
std::vector<Cloud> clouds;

// --- Helper Functions ---
float lerp(float a, float b, float t) {
	return a + t * (b - a);
}
Color lerpColor(Color a, Color b, float t) {
	Color c;
	c.r=lerp(a.r,b.r,t);
	c.g=lerp(a.g,b.g,t);
	c.b=lerp(a.b,b.b,t);
	return c;
}
float randFloat(float min, float max) {
	return min + static_cast<float>(rand())/(static_cast<float>(RAND_MAX/(max-min)));
}
Color randomColor() {
	return {randFloat(0.2f,0.9f), randFloat(0.2f,0.9f), randFloat(0.2f,0.9f)};
}
void DrawEllipse(float cx, float cy, float rx, float ry, int num_segments) {
	glBegin(GL_TRIANGLE_FAN);
	glVertex2f(cx,cy);
	for(int i=0; i<=num_segments; ++i) {
		float theta=2.0f*M_PI*float(i)/float(num_segments);
		float x=rx*cosf(theta);
		float y=ry*sinf(theta);
		glVertex2f(x+cx,y+cy);
	}
	glEnd();
}
void DrawCircle(float cx, float cy, float r, int num_segments) {
	DrawEllipse(cx, cy, r, r, num_segments);
}
float getDarknessFactor() {
	if (!ENABLE_DAY_NIGHT_CYCLE) return 0.0f;
	float sunAngle=timeOfDay*M_PI;
	float sunHeightFactor=sin(sunAngle);
	float darkness=1.0f-std::max(0.0f,sunHeightFactor);
	return std::min(1.0f,darkness*1.5f);
}
float moveTowards(float current, float target, float maxDelta) {
	if (fabs(target - current) <= maxDelta) {
		return target;
	}
	return current + ((target > current) ? 1 : -1) * maxDelta;
}
void RenderText(float x, float y, void* font, const std::string& text, Color color) {
	glColor3f(color.r, color.g, color.b);
	glRasterPos2f(x, y);
	for (char c : text) {
		glutBitmapCharacter(font, c);
	}
}
// isNightTime remains simple for logic checks, fading handled separately
bool isNightTime(float currentTimeOfDay) {
	return (currentTimeOfDay >= NIGHT_START_TIME || currentTimeOfDay <= NIGHT_END_TIME);
}

// --- Drawing Functions ---

void DrawSkyAndSunMoon(float time) {
	/* ... Same ... */ Color dayTop= {0.2f,0.6f,0.9f},dayBottom= {0.5f,0.8f,1.0f},nightTop= {0.05f,0.0f,0.15f},nightBottom= {0.1f,0.05f,0.25f},topColor,bottomColor;
	float darkness=getDarknessFactor();
	if(ENABLE_DAY_NIGHT_CYCLE) {
		topColor=lerpColor(dayTop,nightTop,darkness);
		bottomColor=lerpColor(dayBottom,nightBottom,darkness);
	}
	else {
		topColor=dayTop;
		bottomColor=dayBottom;
	}
	glBegin(GL_QUADS);
	glColor3f(topColor.r,topColor.g,topColor.b);
	glVertex2f(0,windowHeight);
	glVertex2f(windowWidth,windowHeight);
	glColor3f(bottomColor.r,bottomColor.g,bottomColor.b);
	glVertex2f(windowWidth,0);
	glVertex2f(0,0);
	glEnd();
	float horizonY=upperFootpathTopY;
	float skyHeight=windowHeight-horizonY,skyWidth=windowWidth,sunRadius=40.0f,moonRadius=30.0f;
	if(ENABLE_DAY_NIGHT_CYCLE) {
		float sunAngle=time*M_PI,sunX=skyWidth*0.5f-skyWidth*0.48f*cos(sunAngle),sunY=horizonY+skyHeight*0.8f*sin(sunAngle),moonAngle=time*M_PI+M_PI,moonX=skyWidth*0.5f-skyWidth*0.48f*cos(moonAngle),moonY=horizonY+skyHeight*0.8f*sin(moonAngle);
		if(sin(sunAngle)>0.05) {
			glColor3f(1.0f,1.0f,0.1f);
			DrawCircle(sunX,sunY,sunRadius,30);
		}
		if(sin(moonAngle)>0.05) {
			glColor3f(0.9f,0.9f,0.95f);
			DrawCircle(moonX,moonY,moonRadius,30);
			glColor3f(0.7f,0.7f,0.75f);
			DrawCircle(moonX+moonRadius*0.3f,moonY+moonRadius*0.1f,moonRadius*0.2f,10);
			DrawCircle(moonX-moonRadius*0.4f,moonY-moonRadius*0.2f,moonRadius*0.15f,10);
		}
	}
	else {
		glColor3f(1.0f,1.0f,0.0f);
		DrawCircle(windowWidth*0.5f,windowHeight*0.85f,sunRadius,30);
	}
}
void DrawMountains() {
	/* ... Same ... */ float darkness=getDarknessFactor();
	Color baseColorDay= {0.1f,0.35f,0.1f},baseColorNight= {0.02f,0.08f,0.02f},baseColor=lerpColor(baseColorDay,baseColorNight,darkness);
	Color midColorDay= {0.15f,0.45f,0.15f},midColorNight= {0.03f,0.12f,0.03f},midColor=lerpColor(midColorDay,midColorNight,darkness);
	Color topColorDay= {0.2f,0.5f,0.2f},topColorNight= {0.05f,0.15f,0.05f},topColor=lerpColor(topColorDay,topColorNight,darkness);
	glColor3f(baseColor.r,baseColor.g,baseColor.b);
	glBegin(GL_POLYGON);
	glVertex2f(windowWidth*0.3f,upperFootpathTopY);
	glVertex2f(windowWidth*0.45f,windowHeight*0.55f);
	glVertex2f(windowWidth*0.6f,windowHeight*0.4f);
	glVertex2f(windowWidth*0.7f,windowHeight*0.65f);
	glVertex2f(windowWidth*0.9f,windowHeight*0.5f);
	glVertex2f(windowWidth*1.1f,upperFootpathTopY);
	glEnd();
	glColor3f(midColor.r,midColor.g,midColor.b);
	glBegin(GL_POLYGON);
	glVertex2f(windowWidth*0.45f,upperFootpathTopY);
	glVertex2f(windowWidth*0.6f,windowHeight*0.5f);
	glVertex2f(windowWidth*0.75f,windowHeight*0.35f);
	glVertex2f(windowWidth*0.85f,windowHeight*0.6f);
	glVertex2f(windowWidth*1.0f,upperFootpathTopY);
	glEnd();
	glColor3f(topColor.r,topColor.g,topColor.b);
	glBegin(GL_POLYGON);
	glVertex2f(windowWidth*0.65f,upperFootpathTopY);
	glVertex2f(windowWidth*0.8f,windowHeight*0.45f);
	glVertex2f(windowWidth*0.95f,upperFootpathTopY);
	glEnd();
}
void DrawFootpath() {
	/* ... Same ... */ float darkness = getDarknessFactor();
	Color pathDay = {0.7f, 0.7f, 0.7f};
	Color pathNight = {0.3f, 0.3f, 0.3f};
	Color pathColor = lerpColor(pathDay, pathNight, darkness);
	glColor3f(pathColor.r, pathColor.g, pathColor.b);
	glBegin(GL_QUADS);
	glVertex2f(0, upperFootpathTopY);
	glVertex2f(windowWidth, upperFootpathTopY);
	glVertex2f(windowWidth, upperFootpathBottomY);
	glVertex2f(0, upperFootpathBottomY);
	glEnd();
	glBegin(GL_QUADS);
	glVertex2f(0, lowerFootpathTopY);
	glVertex2f(windowWidth, lowerFootpathTopY);
	glVertex2f(windowWidth, lowerFootpathBottomY);
	glVertex2f(0, lowerFootpathBottomY);
	glEnd();
}
void DrawRoad() {
	/* ... Same ... */ float darkness=getDarknessFactor();
	Color roadColorDay= {0.3f,0.3f,0.3f},roadColorNight= {0.1f,0.1f,0.1f},roadColor=lerpColor(roadColorDay,roadColorNight,darkness);
	Color lineDay= {0.9f,0.9f,0.9f},lineNight= {0.4f,0.4f,0.4f},lineColor=lerpColor(lineDay,lineNight,darkness);
	glColor3f(roadColor.r,roadColor.g,roadColor.b);
	glBegin(GL_QUADS);
	glVertex2f(0,roadTopY);
	glVertex2f(windowWidth,roadTopY);
	glVertex2f(windowWidth,roadBottomY);
	glVertex2f(0,roadBottomY);
	glEnd();
	glColor3f(lineColor.r,lineColor.g,lineColor.b);
	glLineWidth(3.0f);
	glBegin(GL_LINES);
	float dashLength=40.0f,gapLength=30.0f,lineY=(roadTopY+roadBottomY)/2.0f;
	float startOffset=(vehicles.empty()?0.0f:fmod(-timeOfDay*50.0f,dashLength+gapLength));
	for(float x=startOffset-(dashLength+gapLength); x<windowWidth; x+=dashLength+gapLength) {
		glVertex2f(x,lineY);
		glVertex2f(x+dashLength,lineY);
	}
	glEnd();
	glLineWidth(1.0f);
}
void DrawZebraCrossing() {
	/* ... Same ... */ float darkness=getDarknessFactor();
	Color stripeDay= {0.9f,0.9f,0.9f},stripeNight= {0.5f,0.5f,0.5f},stripeColor=lerpColor(stripeDay,stripeNight,darkness);
	glColor3f(stripeColor.r,stripeColor.g,stripeColor.b);
	float stripeWidth=8.0f,gapWidth=6.0f,startY=roadBottomY+2,endY=roadTopY-2,startX=zebraCrossingX-zebraCrossingWidth/2.0f;
	for(float x=startX; x<startX+zebraCrossingWidth; x+=stripeWidth+gapWidth) {
		glBegin(GL_QUADS);
		glVertex2f(x,endY);
		glVertex2f(x+stripeWidth,endY);
		glVertex2f(x+stripeWidth,startY);
		glVertex2f(x,startY);
		glEnd();
	}
}
void DrawBuilding1(float x, float y, float scale) {
	/* ... Same ... */ float baseW=60*scale, baseH=250*scale, topH=40*scale;
	float darkness=getDarknessFactor();
	Color mainDay= {0.7f,0.7f,0.2f}, mainNight= {0.3f,0.3f,0.1f}, mainColor=lerpColor(mainDay,mainNight,darkness);
	Color accentDay= {0.2f,0.6f,0.4f}, accentNight= {0.1f,0.3f,0.2f}, accentColor=lerpColor(accentDay,accentNight,darkness);
	bool isNight=isNightTime(timeOfDay);
	Color windowDay= {0.1f,0.1f,0.1f}, windowNight= {0.8f,0.8f,0.5f}, windowColor=isNight?windowNight:windowDay;
	glColor3f(mainColor.r,mainColor.g,mainColor.b);
	glBegin(GL_QUADS);
	glVertex2f(x,y+baseH);
	glVertex2f(x+baseW,y+baseH);
	glVertex2f(x+baseW,y);
	glVertex2f(x,y);
	glEnd();
	glColor3f(accentColor.r,accentColor.g,accentColor.b);
	glBegin(GL_QUADS);
	glVertex2f(x-baseW*0.3f,y+baseH*0.9f);
	glVertex2f(x,y+baseH);
	glVertex2f(x,y);
	glVertex2f(x-baseW*0.3f,y+baseH*0.1f);
	glEnd();
	glBegin(GL_TRIANGLES);
	glVertex2f(x-baseW*0.3f,y+baseH*0.9f);
	glVertex2f(x-baseW*0.15f,y+baseH*0.9f+topH);
	glVertex2f(x,y+baseH);
	glEnd();
	glColor3f(windowColor.r,windowColor.g,windowColor.b);
	int rows=10,cols=3;
	float winW=baseW/cols*0.6f,winH=baseH/rows*0.6f;
	for(int r=0; r<rows; ++r) for(int c=0; c<cols; ++c) {
			float winX=x+(c+0.2f)*(baseW/cols),winY=y+(r+0.2f)*(baseH/rows);
			glRectf(winX,winY,winX+winW,winY+winH);
		}
}
void DrawBuilding2(float x, float y, float scale) {
	/* ... Same ... */ float baseW=80*scale, baseH=300*scale, topH=60*scale;
	float darkness=getDarknessFactor();
	Color mainDay= {0.9f,0.9f,0.9f}, mainNight= {0.4f,0.4f,0.4f}, mainColor=lerpColor(mainDay,mainNight,darkness);
	Color frameDay= {0.1f,0.1f,0.1f}, frameNight= {0.05f,0.05f,0.05f}, frameColor=lerpColor(frameDay,frameNight,darkness);
	bool isNight=isNightTime(timeOfDay);
	Color windowDay= {0.4f,0.5f,0.6f}, windowNight= {0.8f,0.8f,0.5f}, windowColor=isNight?windowNight:windowDay;
	glColor3f(windowColor.r,windowColor.g,windowColor.b);
	glBegin(GL_QUADS);
	glVertex2f(x,y+baseH);
	glVertex2f(x+baseW,y+baseH);
	glVertex2f(x+baseW,y);
	glVertex2f(x,y);
	glEnd();
	glColor3f(frameColor.r,frameColor.g,frameColor.b);
	glLineWidth(2.0f);
	int vLines=6;
	for(int i=0; i<=vLines; ++i) {
		float lineX=x+i*(baseW/vLines);
		glBegin(GL_LINES);
		glVertex2f(lineX,y);
		glVertex2f(lineX,y+baseH);
		glEnd();
	}
	int hLines=15;
	for(int i=0; i<=hLines; ++i) {
		float lineY=y+i*(baseH/hLines);
		glBegin(GL_LINES);
		glVertex2f(x,lineY);
		glVertex2f(x+baseW,lineY);
		glEnd();
	}
	glLineWidth(1.0f);
	glColor3f(mainColor.r,mainColor.g,mainColor.b);
	glBegin(GL_TRIANGLES);
	glVertex2f(x,y+baseH);
	glVertex2f(x+baseW/2,y+baseH+topH);
	glVertex2f(x+baseW,y+baseH);
	glEnd();
	glColor3f(frameColor.r,frameColor.g,frameColor.b);
	glBegin(GL_LINES);
	glVertex2f(x+baseW/2,y+baseH);
	glVertex2f(x+baseW/2,y+baseH+topH);
	glEnd();
}
void DrawBuilding3(float x, float y, float scale) {
	/* ... Same ... */ float currentW=100*scale, currentH=60*scale, currentY=y;
	int segments=6;
	float darkness=getDarknessFactor();
	Color mainDay= {0.2f,0.4f,0.7f}, mainNight= {0.1f,0.2f,0.35f}, mainColor=lerpColor(mainDay,mainNight,darkness);
	bool isNight=isNightTime(timeOfDay);
	Color windowDay= {0.9f,0.5f,0.1f}, windowNight= {1.0f,0.8f,0.3f}, windowColor=isNight?windowNight:windowDay;
	for(int i=0; i<segments; ++i) {
		glColor3f(mainColor.r,mainColor.g,mainColor.b);
		glBegin(GL_QUADS);
		glVertex2f(x,currentY+currentH);
		glVertex2f(x+currentW,currentY+currentH);
		glVertex2f(x+currentW,currentY);
		glVertex2f(x,currentY);
		glEnd();
		glColor3f(windowColor.r,windowColor.g,windowColor.b);
		int numWindows=5-i;
		float winW=currentW*0.1f, winH=currentH*0.5f, winY=currentY+currentH*0.25f;
		float spacing=(currentW-numWindows*winW)/(numWindows+1);
		for(int w=0; w<numWindows; ++w) {
			float winX=x+spacing*(w+1)+winW*w;
			glRectf(winX,winY,winX+winW,winY+winH);
		}
		currentY+=currentH;
		x+=currentW*0.1f;
		currentW*=0.8f;
		currentH*=0.95f;
	}
	glColor3f(0.5f,0.5f,0.5f);
	glBegin(GL_QUADS);
	glVertex2f(x+currentW/2-2*scale,currentY+20*scale);
	glVertex2f(x+currentW/2+2*scale,currentY+20*scale);
	glVertex2f(x+currentW/2+2*scale,currentY);
	glVertex2f(x+currentW/2-2*scale,currentY);
	glEnd();
}
void DrawControlTower(float x, float y, float scale) {
	/* ... Same ... */ float baseH=80*scale, baseW=20*scale, platform1R=40*scale, platform1H=10*scale;
	float platform2R=30*scale, platform2H=8*scale, topR=10*scale;
	float darkness=getDarknessFactor();
	Color baseDay= {0.4f,0.4f,0.45f}, baseNight= {0.15f,0.15f,0.2f}, baseColor=lerpColor(baseDay,baseNight,darkness);
	Color plat1Day= {0.6f,0.6f,0.6f}, plat1Night= {0.3f,0.3f,0.3f}, plat1Color=lerpColor(plat1Day,plat1Night,darkness);
	Color plat2Day= {0.8f,0.8f,0.3f}, plat2Night= {0.4f,0.4f,0.15f}, plat2Color=lerpColor(plat2Day,plat2Night,darkness);
	Color topDay= {0.3f,0.8f,0.8f}, topNight= {0.15f,0.4f,0.4f}, topColor=lerpColor(topDay,topNight,darkness);
	glColor3f(baseColor.r,baseColor.g,baseColor.b);
	glBegin(GL_QUADS);
	glVertex2f(x-baseW/2,y+baseH);
	glVertex2f(x+baseW/2,y+baseH);
	glVertex2f(x+baseW/2,y);
	glVertex2f(x-baseW/2,y);
	glEnd();
	float plat1Y=y+baseH;
	glColor3f(plat1Color.r,plat1Color.g,plat1Color.b);
	glBegin(GL_QUADS);
	glVertex2f(x-platform1R,plat1Y+platform1H);
	glVertex2f(x+platform1R,plat1Y+platform1H);
	glVertex2f(x+platform1R,plat1Y);
	glVertex2f(x-platform1R,plat1Y);
	glEnd();
	float plat2Y=plat1Y+platform1H;
	glColor3f(plat2Color.r,plat2Color.g,plat2Color.b);
	glBegin(GL_QUADS);
	glVertex2f(x-platform2R,plat2Y+platform2H);
	glVertex2f(x+platform2R,plat2Y+platform2H);
	glVertex2f(x+platform2R,plat2Y);
	glVertex2f(x-platform2R,plat2Y);
	glEnd();
	float topY=plat2Y+platform2H;
	glColor3f(topColor.r,topColor.g,topColor.b);
	DrawCircle(x,topY+topR,topR,20);
	glColor3f(0.8f,0.8f,0.8f);
	glBegin(GL_LINES);
	glVertex2f(x,topY+topR*2);
	glVertex2f(x,topY+topR*2+10*scale);
	glEnd();
}
void DrawTrafficLight(float x, float y, float scale) {
	/* ... Same ... */ float poleW=8*scale, poleH=70*scale, boxW=25*scale, boxH=60*scale, lightR=7*scale;
	glColor3f(0.2f,0.2f,0.2f);
	glBegin(GL_QUADS);
	glVertex2f(x-poleW/2,y+poleH);
	glVertex2f(x+poleW/2,y+poleH);
	glVertex2f(x+poleW/2,y);
	glVertex2f(x-poleW/2,y);
	glEnd();
	float boxY=y+poleH*0.3f;
	glColor3f(0.1f,0.1f,0.1f);
	glBegin(GL_QUADS);
	glVertex2f(x-boxW/2,boxY+boxH);
	glVertex2f(x+boxW/2,boxY+boxH);
	glVertex2f(x+boxW/2,boxY);
	glVertex2f(x-boxW/2,boxY);
	glEnd();
	float lightSpacing=boxH/4.0f, lightX=x;
	float redY=boxY+lightSpacing*3, yellowY=boxY+lightSpacing*2, greenY=boxY+lightSpacing*1;
	glColor3f(0.3f,0.0f,0.0f);
	DrawCircle(lightX,redY,lightR,15);
	glColor3f(0.3f,0.3f,0.0f);
	DrawCircle(lightX,yellowY,lightR,15);
	glColor3f(0.0f,0.3f,0.0f);
	DrawCircle(lightX,greenY,lightR,15);
	switch(trafficLightState) {
	case RED:
		glColor3f(1.0f,0.0f,0.0f);
		DrawCircle(lightX,redY,lightR,15);
		break;
	case YELLOW:
		glColor3f(1.0f,1.0f,0.0f);
		DrawCircle(lightX,yellowY,lightR,15);
		break;
	case GREEN:
		glColor3f(0.0f,1.0f,0.0f);
		DrawCircle(lightX,greenY,lightR,15);
		break;
	}
}
void DrawVehicle(const Vehicle& v) { // *** Fading Headlights ***
	float darkness=getDarknessFactor();
	Color bodyColor=lerpColor(v.color, {v.color.r*0.4f,v.color.g*0.4f,v.color.b*0.4f},darkness);
	Color windowColor=lerpColor({0.2f,0.2f,0.3f}, {0.1f,0.1f,0.1f},darkness*0.8f);
	Color wheelColor=lerpColor({0.1f,0.1f,0.1f}, {0.05f,0.05f,0.05f},darkness);
	Color hubcapColor=lerpColor({0.6f,0.6f,0.6f}, {0.3f,0.3f,0.3f},darkness);
	float wheelR=v.height*0.2f;
	// Calculate headlight brightness based on darkness factor
	float headlightBrightness = std::max(0.0f, (darkness - 0.5f) * 2.0f); // Start fading in after half dark
	headlightBrightness = std::min(1.0f, headlightBrightness);
	Color headLightColorDay = {0.3f, 0.3f, 0.3f};
	Color headLightColorNight = {1.0f, 1.0f, 0.7f};
	Color headLightColor = lerpColor(headLightColorDay, headLightColorNight, headlightBrightness);
	float headLightSize=4.0f;
	glPushMatrix();
	glTranslatef(v.x,v.y,0.0f);
	glColor3f(bodyColor.r,bodyColor.g,bodyColor.b);
	glBegin(GL_QUADS);
	if(v.type==BUS) {
		glVertex2f(0,v.height);
		glVertex2f(v.width,v.height);
		glVertex2f(v.width*0.98f,0);
		glVertex2f(v.width*0.02f,0);
	}
	else if(v.type==TRUCK) {
		float cabW=v.width*0.4f,cabH=v.height*0.9f;
		if(v.direction<0) {
			glVertex2f(0,v.height);
			glVertex2f(cabW,v.height);
			glVertex2f(cabW,v.height-cabH);
			glVertex2f(0,v.height-cabH);
			glVertex2f(cabW*1.1f,v.height*0.85f);
			glVertex2f(v.width,v.height*0.85f);
			glVertex2f(v.width,0);
			glVertex2f(cabW*1.1f,0);
		}
		else {
			glVertex2f(v.width-cabW,v.height);
			glVertex2f(v.width,v.height);
			glVertex2f(v.width,v.height-cabH);
			glVertex2f(v.width-cabW,v.height-cabH);
			glVertex2f(0,v.height*0.85f);
			glVertex2f(v.width-cabW*1.1f,v.height*0.85f);
			glVertex2f(v.width-cabW*1.1f,0);
			glVertex2f(0,0);
		}
	}
	else {
		glVertex2f(v.width*0.1f,v.height);
		glVertex2f(v.width*0.9f,v.height);
		glVertex2f(v.width,v.height*0.5f);
		glVertex2f(v.width,0);
		glVertex2f(0,0);
		glVertex2f(0,v.height*0.5f);
	}
	glEnd();
	glColor3f(windowColor.r,windowColor.g,windowColor.b);
	if(v.type==BUS) {
		float winH=v.height*0.4f,winY=v.height*0.4f,winW=v.width*0.12f,spacing=v.width*0.04f;
		for(int i=0; i<5; ++i) glRectf(v.width*0.1f+i*(winW+spacing),winY,v.width*0.1f+i*(winW+spacing)+winW,winY+winH);
		glRectf(v.width*0.1f+5*(winW+spacing),winY,v.width*0.9f,winY+winH);
	}
	else if(v.type==TRUCK) {
		if(v.direction<0) glRectf(v.width*0.05f,v.height*0.4f,v.width*0.35f,v.height*0.9f);
		else glRectf(v.width*0.65f,v.height*0.4f,v.width*0.95f,v.height*0.9f);
	}
	else {
		glBegin(GL_QUADS);
		glVertex2f(v.width*0.15f,v.height*0.9f);
		glVertex2f(v.width*0.85f,v.height*0.9f);
		glVertex2f(v.width*0.9f,v.height*0.5f);
		glVertex2f(v.width*0.1f,v.height*0.5f);
		glEnd();
	}
	glColor3f(wheelColor.r,wheelColor.g,wheelColor.b);
	float frontWheelX, backWheelX;
	if(v.type==TRUCK) {
		if(v.direction<0) {
			frontWheelX=v.width*0.2f;
			backWheelX=v.width*0.8f;
		}
		else {
			frontWheelX=v.width*0.8f;
			backWheelX=v.width*0.2f;
		}
		DrawCircle(frontWheelX,wheelR,wheelR,15);
		DrawCircle(backWheelX,wheelR,wheelR,15);
		DrawCircle(backWheelX + (v.direction<0 ? wheelR*2.2f : -wheelR*2.2f), wheelR, wheelR, 15);
	}
	else {
		if(v.direction<0) {
			frontWheelX=v.width*0.25f;
			backWheelX=v.width*0.75f;
		}
		else {
			frontWheelX=v.width*0.75f;
			backWheelX=v.width*0.25f;
		}
		DrawCircle(frontWheelX,wheelR,wheelR,15);
		DrawCircle(backWheelX,wheelR,wheelR,15);
	}
	glColor3f(hubcapColor.r,hubcapColor.g,hubcapColor.b);
	DrawCircle(frontWheelX,wheelR,wheelR*0.4f,8);
	DrawCircle(backWheelX,wheelR,wheelR*0.4f,8);
	if(v.type==TRUCK) DrawCircle(backWheelX + (v.direction<0 ? wheelR*2.2f : -wheelR*2.2f), wheelR, wheelR*0.4f, 8);
	// Draw Headlights using calculated color
	glColor3f(headLightColor.r, headLightColor.g, headLightColor.b);
	if(v.direction>0) {
		if(v.type!=TRUCK) {
			glRectf(v.width-headLightSize-3,v.height*0.2f,v.width-3,v.height*0.2f+headLightSize);
			glRectf(v.width-headLightSize*2.5f-3,v.height*0.2f,v.width-headLightSize*1.5f-3,v.height*0.2f+headLightSize);
		}
		else {
			glRectf(v.width-headLightSize-3,v.height*0.3f,v.width-3,v.height*0.3f+headLightSize);
		}
	}
	else {
		if(v.type!=TRUCK) {
			glRectf(3,v.height*0.2f,3+headLightSize,v.height*0.2f+headLightSize);
			glRectf(3+headLightSize*1.5f,v.height*0.2f,3+headLightSize*2.5f,v.height*0.2f+headLightSize);
		}
		else {
			glRectf(3,v.height*0.3f,3+headLightSize,v.height*0.3f+headLightSize);
		}
	}
	glPopMatrix();
}
void DrawBird(const Bird& bird) {
	/* ... Same ... */ float darkness=getDarknessFactor()*0.8f;
	Color birdColor=lerpColor({0.1f,0.1f,0.1f}, {0.05f,0.05f,0.05f},darkness);
	Color beakColor=lerpColor({1.0f,0.2f,0.1f}, {0.5f,0.1f,0.05f},darkness);
	Color wingColor=lerpColor({0.9f,0.9f,0.9f}, {0.5f,0.5f,0.5f},darkness);
	glPushMatrix();
	glTranslatef(bird.x,bird.y,0.0f);
	glScalef(0.8f,0.8f,1.0f);
	glColor3f(birdColor.r,birdColor.g,birdColor.b);
	glBegin(GL_POLYGON);
	glVertex2f(-15,0);
	glVertex2f(0,5);
	glVertex2f(10,3);
	glVertex2f(15,-2);
	glVertex2f(0,-5);
	glEnd();
	glColor3f(beakColor.r,beakColor.g,beakColor.b);
	glBegin(GL_TRIANGLES);
	glVertex2f(15,-2);
	glVertex2f(22,0);
	glVertex2f(15,1);
	glEnd();
	float wingYOffset=2.0f, wingTipY;
	float phase=fmod(bird.flapPhase,2.0f*M_PI);
	wingTipY=10.0f+5.0f*sin(phase);
	glColor3f(birdColor.r,birdColor.g,birdColor.b);
	glBegin(GL_TRIANGLES);
	glVertex2f(-8,wingYOffset);
	glVertex2f(8,wingYOffset);
	glVertex2f(0,wingYOffset+wingTipY);
	glEnd();
	glColor3f(wingColor.r,wingColor.g,wingColor.b);
	glBegin(GL_TRIANGLES);
	glVertex2f(-3,wingYOffset+wingTipY*0.7f);
	glVertex2f(3,wingYOffset+wingTipY*0.7f);
	glVertex2f(0,wingYOffset+wingTipY);
	glEnd();
	glPopMatrix();
}
void DrawPedestrian(const Pedestrian& p) { // *** Use darknessFactor for fading alpha ***
	float darkness=getDarknessFactor();
	float alpha = 1.0f;
	if (p.state == WALKING_SIDEWALK) {
		// Start fading when darkness > 0.5, fully faded (alpha=0.1) when darkness = 1.0
		alpha = 1.0f - std::max(0.0f, std::min(1.0f, (darkness - 0.5f) * 2.0f)) * 0.9f;
	}

	Color skinColor=lerpColor({0.9f,0.7f,0.5f}, {0.5f,0.4f,0.3f},darkness);
	Color clothesColor=lerpColor(p.clothingColor, {p.clothingColor.r*0.4f,p.clothingColor.g*0.4f,p.clothingColor.b*0.4f},darkness);
	float headR=4.0f, bodyH=12.0f, bodyW=5.0f, legH=8.0f, legW=2.0f;
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glPushMatrix();
	glTranslatef(p.x,p.y,0.0f);
	glColor4f(skinColor.r,skinColor.g,skinColor.b,alpha);
	DrawCircle(0,bodyH+legH+headR,headR,10);
	glColor4f(clothesColor.r,clothesColor.g,clothesColor.b,alpha);
	glBegin(GL_QUADS);
	glVertex2f(-bodyW/2,legH+bodyH);
	glVertex2f(bodyW/2,legH+bodyH);
	glVertex2f(bodyW/2,legH);
	glVertex2f(-bodyW/2,legH);
	glEnd();
	float legOffset=2.5f*sin(p.legPhase);
	glBegin(GL_QUADS);
	glVertex2f(-legW*1.5f,legH);
	glVertex2f(-legW*0.5f,legH);
	glVertex2f(-legW*0.5f+legOffset,0);
	glVertex2f(-legW*1.5f+legOffset,0);
	glEnd();
	glBegin(GL_QUADS);
	glVertex2f(legW*0.5f,legH);
	glVertex2f(legW*1.5f,legH);
	glVertex2f(legW*1.5f-legOffset,0);
	glVertex2f(legW*0.5f-legOffset,0);
	glEnd();
	glPopMatrix();
}
void DrawTree(const Tree& tree) {
	/* ... Same ... */ float darkness=getDarknessFactor();
	Color currentFoliageColor=lerpColor(tree.foliageColor, {tree.foliageColor.r*0.3f,tree.foliageColor.g*0.3f,tree.foliageColor.b*0.3f},darkness);
	Color currentTrunkColor=lerpColor(tree.trunkColor, {tree.trunkColor.r*0.3f,tree.trunkColor.g*0.3f,tree.trunkColor.b*0.3f},darkness);
	float trunkWidth=10.0f*tree.scale,trunkHeight=40.0f*tree.scale,foliageRadius=25.0f*tree.scale,foliageCenterY=tree.pos.y+trunkHeight;
	glColor3f(currentTrunkColor.r,currentTrunkColor.g,currentTrunkColor.b);
	glBegin(GL_QUADS);
	glVertex2f(tree.pos.x-trunkWidth/2,tree.pos.y+trunkHeight);
	glVertex2f(tree.pos.x+trunkWidth/2,tree.pos.y+trunkHeight);
	glVertex2f(tree.pos.x+trunkWidth/2,tree.pos.y);
	glVertex2f(tree.pos.x-trunkWidth/2,tree.pos.y);
	glEnd();
	glColor3f(currentFoliageColor.r,currentFoliageColor.g,currentFoliageColor.b);
	DrawCircle(tree.pos.x,foliageCenterY,foliageRadius,20);
	DrawCircle(tree.pos.x-foliageRadius*0.4f,foliageCenterY+foliageRadius*0.1f,foliageRadius*0.7f,15);
	DrawCircle(tree.pos.x+foliageRadius*0.4f,foliageCenterY+foliageRadius*0.1f,foliageRadius*0.7f,15);
	DrawCircle(tree.pos.x,foliageCenterY+foliageRadius*0.5f,foliageRadius*0.6f,15);
}
void DrawStreetLight(const StreetLight& light) { // *** Simplified Glow ***
	float poleWidth = 5.0f;
	float lampHeight = 4.0f;
	float lampWidth = 10.0f;
	float armAngle = light.onUpper ? -25.0f : 25.0f;
	float darkness = getDarknessFactor();
	Color roadColorDay= {0.3f,0.3f,0.3f},roadColorNight= {0.1f,0.1f,0.1f},poleColor=lerpColor(roadColorDay,roadColorNight,darkness);
	Color lampColorOff= {0.2f,0.2f,0.2f},lampColorOn= {1.0f,0.95f,0.75f};
	// Calculate lamp brightness based on darkness factor
	float lampBrightness = std::max(0.0f, std::min(1.0f, (darkness - 0.4f) * (1.0f / 0.5f) )); // Fade in between darkness 0.4 and 0.9
	Color lampColor = lerpColor(lampColorOff, lampColorOn, lampBrightness);

	glColor3f(poleColor.r, poleColor.g, poleColor.b);
	glBegin(GL_QUADS);
	glVertex2f(light.pos.x-poleWidth/2,light.pos.y+light.height);
	glVertex2f(light.pos.x+poleWidth/2,light.pos.y+light.height);
	glVertex2f(light.pos.x+poleWidth/2,light.pos.y);
	glVertex2f(light.pos.x-poleWidth/2,light.pos.y);
	glEnd();
	glPushMatrix();
	glTranslatef(light.pos.x,light.pos.y+light.height,0.0f);
	glRotatef(armAngle,0,0,1);
	glColor3f(poleColor.r,poleColor.g,poleColor.b);
	glLineWidth(3.0f);
	glBegin(GL_LINES);
	glVertex2f(0,0);
	glVertex2f(light.onUpper?-light.armLength:light.armLength,0);
	glEnd();
	glLineWidth(1.0f);
	float lampPosX=light.onUpper?-light.armLength:light.armLength;
	float lampPosY=-lampHeight*0.5f;
	glColor3f(lampColor.r,lampColor.g,lampColor.b);
	glRectf(lampPosX-lampWidth/2,lampPosY-lampHeight/2,lampPosX+lampWidth/2,lampPosY+lampHeight/2);

	// Glow Effect (Fades with lamp brightness)
	if (lampBrightness > 0.01f) { // Only draw glow if lamp is somewhat on
		float angleRad = armAngle * M_PI / 180.0f;
		float lampWorldX = light.pos.x + cos(angleRad)*lampPosX;
		float lampWorldY = light.pos.y + light.height + sin(angleRad)*lampPosX + lampPosY;
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		int glowSegments = 20;
		float maxRadius = 70.0f;
		glBegin(GL_TRIANGLE_FAN);
		glColor4f(1.0f, 0.95f, 0.7f, 0.25f * lampBrightness); // Center alpha depends on brightness
		glVertex2f(lampWorldX, lampWorldY);
		glColor4f(1.0f, 0.9f, 0.6f, 0.0f); // Edge always transparent
		for (int i = 0; i <= glowSegments; ++i) {
			float angle = M_PI + M_PI * (float)i / (float)glowSegments;
			glVertex2f(lampWorldX + maxRadius * cos(angle) * 0.7f, lampWorldY + maxRadius * sin(angle) * 1.1f);
		}
		glEnd();
	}
	glPopMatrix();
}
void DrawClouds(const Cloud& cloud) { // Takes a single cloud
	// Cloud color with fading alpha
	glColor4f(1.0f, 1.0f, 1.0f, cloud.alpha); // Use cloud's alpha

	glPushMatrix();
	glTranslatef(cloud.pos.x, cloud.pos.y, 0.0f);
	glScalef(cloud.scale, cloud.scale, 1.0f);
	glEnable(GL_BLEND); // Ensure blend is on for clouds
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	for (int i = 0; i < cloud.numEllipses; ++i) {
		float shapeChangeX=1.0f+0.05f*sin(cloud.shapePhase+i*0.8f);
		float shapeChangeY=1.0f+0.05f*cos(cloud.shapePhase+i*1.1f);
		DrawEllipse(cloud.ellipseOffsets[i].x, cloud.ellipseOffsets[i].y, cloud.ellipseRadiiX[i]*shapeChangeX, cloud.ellipseRadiiY[i]*shapeChangeY, 15);
	}
	glPopMatrix();
}

// --- Update and Initialization ---

void InitializeClouds() {
	clouds.clear();
	float cloudBaseY=windowHeight*0.75f;
	for(int i=0; i<NUM_CLOUDS; ++i) {
		Cloud c;
		c.pos= {randFloat(-windowWidth*0.2f,windowWidth*1.2f),cloudBaseY+randFloat(-windowHeight*0.05f,windowHeight*0.1f)};
		c.speed=randFloat(0.1f,0.4f);
		c.scale=randFloat(0.8f,1.6f);
		c.numEllipses=3+rand()%3;
		c.shapePhase=randFloat(0,2.0f*M_PI);
		c.alpha = 0.0f; /*Start invisible*/ float totalWidth=0;
		for(int j=0; j<c.numEllipses; ++j) {
			float offsetX=totalWidth+randFloat(-5,5);
			float offsetY=randFloat(-8,8);
			float radiusX=randFloat(25,40);
			float radiusY=randFloat(15,30);
			c.ellipseOffsets.push_back({offsetX,offsetY});
			c.ellipseRadiiX.push_back(radiusX);
			c.ellipseRadiiY.push_back(radiusY);
			totalWidth+=radiusX*randFloat(0.6f,0.9f);
		}
		for(int j=0; j<c.numEllipses; ++j) {
			c.ellipseOffsets[j].x-=totalWidth/2.2f;
		}
		clouds.push_back(c);
	}
}
void InitializeScene() {
	/* ... Calls InitializeClouds ... */ srand(time(0));
	birds.clear();
	int numBirds=3+rand()%3;
	if (!isNightTime(timeOfDay)) {
		for(int i=0; i<numBirds; ++i) {
			Bird b;
			b.x=randFloat(0,windowWidth);
			b.y=birdBaseY+randFloat(-birdAmplitudeY,birdAmplitudeY);
			b.speed=randFloat(0.8f,1.8f);
			b.flapPhase=randFloat(0,2.0f*M_PI);
			b.flapSpeed=randFloat(0.15f,0.35f);
			b.bobPhase=randFloat(0,2.0f*M_PI);
			birds.push_back(b);
		}
	}
	vehicles.clear();
	float initialSpacing=150.0f;
	for(int i=0; i<NUM_CARS; ++i) {
		Vehicle v;
		v.type=(VehicleType)(rand()%3);
		v.color=randomColor();
		bool goRight=(i%2==0);
		v.y=goRight?LANE_Y1:LANE_Y2;
		v.direction=goRight?1:-1;
		switch(v.type) {
		case BUS:
			v.width=100;
			v.height=40;
			v.baseSpeed=randFloat(0.6f,1.0f);
			break;
		case TRUCK:
			v.width=120;
			v.height=45;
			v.baseSpeed=randFloat(0.5f,0.9f);
			break;
		default:
			v.width=60;
			v.height=25;
			v.baseSpeed=randFloat(0.8f,1.6f);
			break;
		}
		v.speed=v.baseSpeed*randFloat(0.5f,1.0f);
		if(v.direction>0) {
			v.x=-200.0f-(i/2)*(v.width+initialSpacing+randFloat(0,50));
		}
		else {
			v.x=windowWidth+200.0f+(i/2)*(v.width+initialSpacing+randFloat(0,50));
		}
		vehicles.push_back(v);
	}
	sidewalkPedestrians.clear();
	for(int i=0; i<NUM_SIDEWALK_PEDESTRIANS; ++i) {
		Pedestrian p;
		p.onUpperPath=(rand()%2==0);
		p.y=p.onUpperPath?upperSidewalkLevelY:lowerSidewalkLevelY;
		p.x=randFloat(0,windowWidth);
		p.speed=randFloat(0.3f,0.7f)*((rand()%2)*2-1);
		p.state=WALKING_SIDEWALK;
		p.legPhase=randFloat(0,2.0f*M_PI);
		p.legSpeed=randFloat(0.08f,0.15f);
		p.clothingColor=randomColor();
		sidewalkPedestrians.push_back(p);
	}
	crossingPedestrians.clear();
	float waitX=crossingWalkX;
	for(int i=0; i<NUM_CROSSING_PEDESTRIANS; ++i) {
		Pedestrian p;
		p.onUpperPath=(i%2==0);
		p.y=p.onUpperPath?upperSidewalkLevelY:lowerSidewalkLevelY;
		p.x=waitX+randFloat(-zebraCrossingWidth*0.3f,zebraCrossingWidth*0.3f);
		p.speed=randFloat(0.5f,0.8f);
		p.state=WAITING_TO_CROSS;
		p.targetY=p.onUpperPath?lowerSidewalkLevelY:upperSidewalkLevelY;
		p.legPhase=randFloat(0,2.0f*M_PI);
		p.legSpeed=randFloat(0.1f,0.18f);
		p.clothingColor=randomColor();
		crossingPedestrians.push_back(p);
	}
	trees.clear();
	Color trunkC= {0.4f,0.2f,0.1f};
	for(int i=0; i<NUM_TREES; ++i) {
		Tree t;
		bool onUpper=(rand()%2==0);
		t.pos.y=onUpper?upperFootpathTopY:lowerFootpathBottomY;
		t.pos.x=randFloat(20,windowWidth-20);
		if(fabs(t.pos.x-zebraCrossingX)<zebraCrossingWidth*1.5) {
			t.pos.x+=(t.pos.x>zebraCrossingX)?zebraCrossingWidth:-zebraCrossingWidth;
		}
		t.scale=randFloat(0.8f,1.3f);
		t.foliageColor= {randFloat(0.0f,0.1f),randFloat(0.3f,0.6f),randFloat(0.0f,0.15f)};
		t.trunkColor=trunkC;
		trees.push_back(t);
	}
	streetLights.clear();
	float poleBaseWidth=5.0f;
	for(int i=0; i<NUM_STREETLIGHTS; ++i) {
		StreetLight sl;
		sl.onUpper=(i%2==0);
		sl.pos.y=sl.onUpper?upperFootpathBottomY:lowerFootpathBottomY;
		sl.height=85.0f+randFloat(-5.0f,5.0f);
		sl.armLength=35.0f+randFloat(-3.0f,8.0f);
		sl.pos.x=(windowWidth/(NUM_STREETLIGHTS+1.0f))*(i+1.0f);
		sl.pos.x+=randFloat(-35.0f,35.0f);
		if(fabs(sl.pos.x-zebraCrossingX)<zebraCrossingWidth*1.2) {
			sl.pos.x+=(sl.pos.x>zebraCrossingX)?zebraCrossingWidth*0.8f:-zebraCrossingWidth*0.8f;
		}
		sl.pos.x=std::max(poleBaseWidth,std::min(windowWidth-poleBaseWidth,sl.pos.x));
		streetLights.push_back(sl);
	}
	InitializeClouds();
}
bool IsCrossingBlocked() {
	/* ... Same ... */ for(const auto& v:vehicles) {
		if(v.x<crossingBackEdge && v.x+v.width>crossingFrontEdge) {
			return true;
		}
	}
	return false;
}

void UpdateScene(int value) {
	bool night = isNightTime(timeOfDay);
	if(ENABLE_DAY_NIGHT_CYCLE) {
		timeOfDay+=timeSpeed;
		if(timeOfDay>=1.0f) timeOfDay-=1.0f;
	}
	trafficLightTimer++;
	bool carsMustStopIntent=false;
	int remainingTimeInPhase=0;
	LightState nextLightState=trafficLightState;
	if(trafficLightState==RED) {
		carsMustStopIntent=true;
		remainingTimeInPhase=RED_DURATION-trafficLightTimer;
		if(trafficLightTimer>=RED_DURATION) {
			nextLightState=GREEN;
		}
	}
	else if(trafficLightState==YELLOW) {
		carsMustStopIntent=true;
		remainingTimeInPhase=YELLOW_DURATION-trafficLightTimer;
		if(trafficLightTimer>=YELLOW_DURATION) {
			nextLightState=RED;
		}
	}
	else {
		carsMustStopIntent=false;
		remainingTimeInPhase=GREEN_DURATION-trafficLightTimer;
		if(trafficLightTimer>=GREEN_DURATION) {
			nextLightState=YELLOW;
		}
	}
	if(nextLightState!=trafficLightState) {
		trafficLightState=nextLightState;
		trafficLightTimer=0;
		if(trafficLightState==RED) {
			carsMustStopIntent=true;
			remainingTimeInPhase=RED_DURATION;
		}
		else if(trafficLightState==YELLOW) {
			carsMustStopIntent=true;
			remainingTimeInPhase=YELLOW_DURATION;
		}
		else {
			carsMustStopIntent=false;
			remainingTimeInPhase=GREEN_DURATION;
		}
	}
	// Update Vehicles (Logic Same)
	for(int i=0; i<vehicles.size(); ++i) {
		Vehicle& v=vehicles[i];
		float relevantStopLine=(v.direction>0)?stopLineLeft:stopLineRight;
		float effectiveFrontX=(v.direction>0)?v.x+v.width:v.x;
		bool shouldConsiderStopping=false;
		if(carsMustStopIntent) {
			shouldConsiderStopping=true;
		}
		else if(trafficLightState==GREEN) {
			float predictionSpeed=std::max(0.5f,v.baseSpeed);
			float distanceToClearCrossing=(v.direction>0)?crossingBackEdge-v.x:(v.x+v.width)-crossingFrontEdge;
			float timeToClear=(predictionSpeed>0.1f)?(fabs(distanceToClearCrossing)/predictionSpeed):9999.0f;
			float decisionPoint=relevantStopLine-v.direction*predictionSpeed*60.0f;
			if((timeToClear*CAR_TIME_PREDICTION_FACTOR>remainingTimeInPhase && ((v.direction>0&&effectiveFrontX>decisionPoint)||(v.direction<0&&effectiveFrontX<decisionPoint))) || (remainingTimeInPhase<40&&fabs(effectiveFrontX-relevantStopLine)<50.0f)) {
				shouldConsiderStopping=true;
			}
		}
		float maxSpeedTraffic=v.baseSpeed;
		if(shouldConsiderStopping) {
			float distToStop=fabs(relevantStopLine-effectiveFrontX);
			bool isBeforeStopLine=(v.direction>0&&effectiveFrontX<relevantStopLine)||(v.direction<0&&effectiveFrontX>relevantStopLine);
			if(!isBeforeStopLine&&fabs(effectiveFrontX-relevantStopLine)<10.0f) {
				maxSpeedTraffic=0.0f;
			}
			else if(isBeforeStopLine) {
				float brakeFactor=std::max(0.0f,std::min(1.0f,distToStop/100.0f));
				maxSpeedTraffic=std::min(maxSpeedTraffic,v.baseSpeed*brakeFactor*brakeFactor);
				maxSpeedTraffic=std::max(0.0f,maxSpeedTraffic);
			}
			else {
				if(v.speed<0.1f) maxSpeedTraffic=0.0f;
			}
			bool frontNearCrossing=(v.direction>0&&effectiveFrontX+v.width>=crossingFrontEdge-2.0f)||(v.direction<0&&effectiveFrontX<=crossingBackEdge+2.0f);
			bool rearBeforeCrossing=(v.direction>0&&v.x<crossingBackEdge)||(v.direction<0&&v.x+v.width>crossingFrontEdge);
			if(isBeforeStopLine&&frontNearCrossing&&rearBeforeCrossing) {
				maxSpeedTraffic = std::min(maxSpeedTraffic, 0.0f);
			}
		}
		float maxSpeedAhead=v.baseSpeed*1.5f;
		float minDistAhead=std::numeric_limits<float>::max();
		int carAheadIndex=-1;
		for(int j=0; j<vehicles.size(); ++j) {
			if(i==j)continue;
			const Vehicle& other=vehicles[j];
			if(fabs(other.y-v.y)<5.0f&&other.direction==v.direction) {
				float dist=std::numeric_limits<float>::max();
				bool isAhead=false;
				if(v.direction>0&&other.x>v.x) {
					dist=other.x-(v.x+v.width);
					isAhead=true;
				}
				else if(v.direction<0&&other.x<v.x) {
					dist=v.x-(other.x+other.width);
					isAhead=true;
				}
				if(isAhead&&dist<minDistAhead) {
					minDistAhead=dist;
					carAheadIndex=j;
				}
			}
		}
		if(carAheadIndex!=-1) {
			float safeDist=CAR_MIN_SAFE_DISTANCE+v.speed*5.0f;
			if(minDistAhead<safeDist) {
				float aheadSpeed=vehicles[carAheadIndex].speed;
				if(minDistAhead<CAR_MIN_SAFE_DISTANCE) {
					maxSpeedAhead=std::min(aheadSpeed*0.8f,v.speed*0.5f);
				}
				else {
					maxSpeedAhead=aheadSpeed;
				}
				maxSpeedAhead=std::max(0.0f,maxSpeedAhead);
			}
		}
		float targetSpeed = std::min(v.baseSpeed,std::min(maxSpeedTraffic,maxSpeedAhead));
		if(v.speed<targetSpeed) {
			v.speed=std::min(targetSpeed,v.speed+CAR_ACCELERATION);
		}
		else if(v.speed>targetSpeed) {
			v.speed=std::max(targetSpeed,v.speed-CAR_DECELERATION);
		}
		v.speed=std::max(0.0f,v.speed);
		v.x+=v.speed*v.direction;
		if(v.direction>0&&v.x>windowWidth+50) {
			v.x=-v.width-randFloat(150,400);
			v.y=LANE_Y1;
			v.direction=1;
			v.color=randomColor();
			v.type=(VehicleType)(rand()%3);
			switch(v.type) {
			case BUS:
				v.width=100;
				v.height=40;
				v.baseSpeed=randFloat(0.6f,1.0f);
				break;
			case TRUCK:
				v.width=120;
				v.height=45;
				v.baseSpeed=randFloat(0.5f,0.9f);
				break;
			default:
				v.width=60;
				v.height=25;
				v.baseSpeed=randFloat(0.8f,1.6f);
				break;
			}
			if(v.speed>0.1f) v.speed=v.baseSpeed*randFloat(0.5f,0.8f);
			else v.speed=0;
		}
		else if(v.direction<0&&v.x+v.width<-50) {
			v.x=windowWidth+50+randFloat(150,400);
			v.y=LANE_Y2;
			v.direction=-1;
			v.color=randomColor();
			v.type=(VehicleType)(rand()%3);
			switch(v.type) {
			case BUS:
				v.width=100;
				v.height=40;
				v.baseSpeed=randFloat(0.6f,1.0f);
				break;
			case TRUCK:
				v.width=120;
				v.height=45;
				v.baseSpeed=randFloat(0.5f,0.9f);
				break;
			default:
				v.width=60;
				v.height=25;
				v.baseSpeed=randFloat(0.8f,1.6f);
				break;
			}
			if(v.speed>0.1f) v.speed=v.baseSpeed*randFloat(0.5f,0.8f);
			else v.speed=0;
		}
	}
	// Update Birds (Logic Same)
	if (!night) {
		for(auto& b:birds) {
			b.x+=b.speed;
			b.flapPhase+=b.flapSpeed;
			if(b.flapPhase>2.0f*M_PI)b.flapPhase-=2.0f*M_PI;
			b.bobPhase+=b.speed*0.01f;
			b.y=birdBaseY+birdAmplitudeY*sin(b.bobPhase);
			if(b.x>windowWidth+50) {
				b.x=-50.0f;
				b.y=birdBaseY+randFloat(-birdAmplitudeY,birdAmplitudeY);
				b.bobPhase=randFloat(0,2.0f*M_PI);
			}
		}
	}
	// Update Sidewalk Pedestrians (Logic Same)
	for(auto& p:sidewalkPedestrians) {
		if(!night) {
			p.x+=p.speed;
			p.legPhase+=p.legSpeed*fabs(p.speed);
			if(p.legPhase>2.0f*M_PI)p.legPhase-=2.0f*M_PI;
			if(p.speed > 0 && p.x > windowWidth + 10) p.x = -10;
			if(p.speed < 0 && p.x < -10) p.x = windowWidth + 10;
		}
		else {
			p.legPhase = 0;
		}
		p.y = p.onUpperPath ? upperSidewalkLevelY : lowerSidewalkLevelY;
	}
	// Update Crossing Pedestrians (Logic Same)
	int crossingCount=0;
	for(const auto& p:crossingPedestrians) {
		if(p.state==CROSSING)crossingCount++;
	}
	for(auto& p:crossingPedestrians) {
		float moveDelta=p.speed;
		float currentLegSpeedFactor=0.1f;
		bool crossingBlockedByCar=false;
		switch(p.state) {
		case WAITING_TO_CROSS:
			if(!night&&trafficLightState==RED&&crossingCount<2) {
				crossingBlockedByCar=IsCrossingBlocked();
				if(!crossingBlockedByCar) {
					p.state=CROSSING;
					crossingCount++;
				}
			}
			break;
		case CROSSING:
			currentLegSpeedFactor=1.0f;
			p.x=moveTowards(p.x,crossingWalkX,moveDelta*0.2f);
			p.y=moveTowards(p.y,p.targetY,moveDelta);
			if(fabs(p.y-p.targetY)<1.0f) {
				p.state=FINISHED_CROSSING;
				p.y=p.targetY;
				p.x=crossingWalkX+randFloat(-zebraCrossingWidth*0.3f,zebraCrossingWidth*0.3f);
			}
			break;
		case FINISHED_CROSSING:
			if(trafficLightState!=RED) {
				p.state=WAITING_TO_CROSS;
				p.onUpperPath=!p.onUpperPath;
				p.targetY=p.onUpperPath?lowerSidewalkLevelY:upperSidewalkLevelY;
				p.x=crossingWalkX+randFloat(-zebraCrossingWidth*0.3f,zebraCrossingWidth*0.3f);
			}
			break;
		case WALKING_SIDEWALK:
			break;
		}
		if(p.state==CROSSING) {
			currentLegSpeedFactor=1.0f;
		}
		p.legPhase+=p.legSpeed*currentLegSpeedFactor;
		if(p.legPhase>2.0f*M_PI)p.legPhase-=2.0f*M_PI;
	}
	// Update Clouds (with alpha fading)
	for (auto& cloud : clouds) {
		if (!night) cloud.pos.x += cloud.speed; // Only move if not night
		cloud.shapePhase += 0.01f;
		if (cloud.shapePhase > 2.0f * M_PI) cloud.shapePhase -= 2.0f * M_PI;

		// Cloud Alpha Fading Logic
		float dawnEndTime = NIGHT_END_TIME + DAWN_DURATION;
		float duskStartTime = NIGHT_START_TIME - DUSK_DURATION;

		if (timeOfDay > NIGHT_END_TIME && timeOfDay < dawnEndTime) { // Fading in (Dawn)
			cloud.alpha = (timeOfDay - NIGHT_END_TIME) / DAWN_DURATION;
		} else if (timeOfDay > duskStartTime && timeOfDay < NIGHT_START_TIME) { // Fading out (Dusk)
			cloud.alpha = 1.0f - (timeOfDay - duskStartTime) / DUSK_DURATION;
		} else if (timeOfDay >= dawnEndTime && timeOfDay <= duskStartTime) { // Full Day
			cloud.alpha = 1.0f;
		} else { // Full Night
			cloud.alpha = 0.0f;
		}
		cloud.alpha = std::max(0.0f, std::min(1.0f, cloud.alpha)); // Clamp alpha

		// Wrapping Logic
		float approxCloudWidth=0;
		for(float r:cloud.ellipseRadiiX) approxCloudWidth+=r*cloud.scale*0.6f;
		if (cloud.pos.x - approxCloudWidth > windowWidth) {
			cloud.pos.x = -approxCloudWidth - randFloat(50, 150);
			cloud.pos.y = windowHeight * 0.75f + randFloat(-windowHeight*0.05f, windowHeight*0.1f);
		}
	}

	glutPostRedisplay();
	glutTimerFunc(16, UpdateScene, 0);
}

// --- OpenGL Display and Setup ---

void display() {
	bool night = isNightTime(timeOfDay);
	glClear(GL_COLOR_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	DrawSkyAndSunMoon(timeOfDay);
	// Draw Clouds (with alpha)
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Ensure blending for clouds
	for(const auto& cloud : clouds) {
		if (cloud.alpha > 0.01f) DrawClouds(cloud);    // Only draw if visible
	}
	// Draw Scenery
	DrawMountains();
	DrawBuilding1(windowWidth*0.1f, upperFootpathTopY, 1.0f);
	DrawBuilding2(windowWidth*0.2f, upperFootpathTopY, 1.0f);
	DrawBuilding3(windowWidth*0.45f, upperFootpathTopY, 1.0f);
	DrawControlTower(windowWidth*0.85f, upperFootpathTopY, 1.0f);
	DrawFootpath();
	for (const auto& sl : streetLights) {
		DrawStreetLight(sl);
	}
	for (const auto& t : trees) {
		DrawTree(t);
	}
	DrawRoad();
	DrawZebraCrossing();
	for(const auto& v:vehicles) {
		DrawVehicle(v);
	}
	DrawTrafficLight(trafficLightX, upperFootpathBottomY, 1.0f);
	for(const auto& p:crossingPedestrians) {
		if(p.state==CROSSING) DrawPedestrian(p);
	}
	for(const auto& p:sidewalkPedestrians) {
		DrawPedestrian(p);
	}
	for(const auto& p:crossingPedestrians) {
		if(p.state==WAITING_TO_CROSS || p.state==FINISHED_CROSSING) DrawPedestrian(p);
	}
	if(!night) {
		for(const auto& bird:birds) {
			DrawBird(bird);    // Only draw birds if not night
		}
	}
	glutSwapBuffers();
}

void reshape(int w, int h) {
	/* ... Same ... */ windowWidth = w;
	windowHeight = h;
	if (h == 0) h = 1;
	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluOrtho2D(0.0, (double)w, 0.0, (double)h);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}
void initGL() {
	/* ... Same ... */ glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
// --- Main Function ---
int main(int argc, char** argv) {
	/* ... Same ... */ glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	glutInitWindowSize(windowWidth, windowHeight);
	glutInitWindowPosition(50, 50);
	glutCreateWindow("Animated City Scenery - Gradual Night");
	initGL();
	InitializeScene();
	glutDisplayFunc(display);
	glutReshapeFunc(reshape);
	glutTimerFunc(25, UpdateScene, 0);
	glutMainLoop();
	return 0;
}
