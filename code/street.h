// defines a street type definition
typedef struct {
  String name;
  // support up to 10 points, since float is 4 bytes
  float polyX[40];
  float polyY[40];
  int numPoints;
  int speedLimit;
} street;
