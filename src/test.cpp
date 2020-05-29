#include <common.h>
#include <sdf.h>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <fstream>
#include <cmath>

GLFWwindow *window;

vec3 lightPosition = vec3(3.f, 3.f, 3.f);
vec3 lightColor = vec3(1.f, 1.f, 1.f);
float lightPower = 1.f;

/* for view control */
float verticalAngle = -2.76603;
float horizontalAngle = 1.56834;
float initialFoV = 45.0f;
float speed = 5.0f;
float mouseSpeed = 0.005f;

mat4 model, view, projection;
vec3 eyePoint = vec3(0.106493, 3.517007, 1.688342);
vec3 eyeDirection =
    vec3(sin(verticalAngle) * cos(horizontalAngle), cos(verticalAngle),
         sin(verticalAngle) * sin(horizontalAngle));
vec3 up = vec3(0.f, 1.f, 0.f);

/* for sdf */
ivec3 nOfCells;
float cellSize = 0.25f;
vec3 gridOrigin(0, 0, 0);
vec3 rangeOffset(0.5f, 0.5f, 0.5f);
Grid grid;

/* opengl variables */
GLuint exeShader;
GLint uniM, uniV, uniP;
GLint uniLightColor, uniLightPosition, uniLightPower;
GLint uniEyePoint;

void computeMatricesFromInputs(mat4 &, mat4 &);
void keyCallback(GLFWwindow *, int, int, int, int);

void initGL();
void initMatrix();
void initLight();
void initShader();
void initGrid();
void releaseResource();

void writePointCloud(vector<vec3> &, const string);
void writeSdf(Grid &, const string);
void readSdf(Grid &, const string);
vec3 calCellPos(vec3);

// random number in [0, 1]
float randf() {
  // srand(clock());

  float f = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);

  return f;
}

int main(int argc, char const *argv[]) {
  initGL();
  initShader();
  initMatrix();
  initLight();

  /* prepare mesh data */
  // Mesh mesh = loadObj("./mesh/sphere.obj");
  // Mesh mesh = loadObj("./mesh/monkey.obj");
  // Mesh mesh = loadObj("./mesh/torus.obj");
  // Mesh mesh = loadObj("./mesh/bunny.obj");
  Mesh mesh = loadObj("./mesh/cube.obj");
  initMesh(mesh);
  findAABB(mesh);

  // transform mesh to (origin + offset) position
  vec3 offset = (gridOrigin - mesh.min) + rangeOffset;
  mesh.translate(offset);
  updateMesh(mesh);

  /* grid parameters */
  // The grid covers the area of mesh
  // Between the grid and the mesh,
  // there is a offset area which is defined by rangeOffset
  vec3 gridSize = (mesh.max + rangeOffset) - gridOrigin;
  nOfCells = ivec3(gridSize / cellSize);

  initGrid();

  /* find a searching range */
  // select an area a little bigger than mesh's aabb
  vec3 rangeMin = mesh.min - rangeOffset;
  vec3 rangeMax = mesh.max + rangeOffset;

  // std::cout << to_string(rangeMin) << '\n';
  // std::cout << to_string(rangeMax) << '\n';

  // find cells which cover those area
  vec3 startCell = calCellPos(rangeMin);
  vec3 endCell = calCellPos(rangeMax);

  // for the selected range
  // for (float z = startCell.z; z < endCell.z; z += cellSize) {
  //   for (float y = startCell.y; y < endCell.y; y += cellSize) {
  //     for (float x = startCell.x; x < endCell.x; x += cellSize) {
  //       vec3 P(x, y, z); // cell position
  //       float dist = 9999.f;
  //
  //       // iterate triangles in the mesh
  //       for (size_t i = 0; i < mesh.faces.size(); i++) {
  //         Face face = mesh.faces[i];
  //
  //         glm::vec3 A, B, C, N;
  //         A = mesh.vertices[face.v1];
  //         B = mesh.vertices[face.v2];
  //         C = mesh.vertices[face.v3];
  //         N = mesh.faceNormals[face.vn1];
  //
  //         float temp = distPoint2Triangle(A, B, C, N, P);
  //         float oldDist = dist;
  //
  //         // for general case
  //         dist = (glm::abs(temp) < glm::abs(dist)) ? temp : dist;
  //
  //         // for a special case
  //         float delta = abs(abs(temp) - abs(oldDist));
  //         // if delta is less than some threshold
  //         // we decide that temp is equal to dist
  //         if (delta < 0.0001f) {
  //
  //           // if dist will change its sign
  //           // we keep dist at the positive one
  //           dist = (temp > 0) ? temp : oldDist;
  //         }
  //       } // end iterate triangles
  //
  //       // write dist into grid
  //       int hash = calCellHash(P, nOfCells, cellSize);
  //       grid.cells[hash].sd = dist;
  //     } // end x direction
  //   }   // end y direction
  // }     // end z direction

  // writeSdf(grid, "sdf.txt");

  // test
  std::vector<Point> pts;
  for (size_t i = 0; i < 30; i++) {
    Point p;
    p.pos = vec3(0, 0.1 * i, 1.25);
    pts.push_back(p);
  }
  // Point p;
  // p.pos = vec3(0, 1.5, 0);
  // p.v = vec3(randf(), randf(), randf());
  // pts.push_back(p);
  //
  //
  // std::vector<vec3> starts, ends;
  // for (size_t i = 0; i < pts.size(); i++) {
  //   vec3 start = pts[i].pos;
  //   vec3 end = pts[i].pos +
  //              (grid.getGradient(pts[i].pos) * grid.getDistance(pts[i].pos));
  //
  //   starts.push_back(start);
  //   ends.push_back(end);
  // }

  /* glfw loop */
  // a rough way to solve cursor position initialization problem
  // must call glfwPollEvents once to activate glfwSetCursorPos
  // this is a glfw mechanism problem
  glfwPollEvents();
  glfwSetCursorPos(window, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);

  for (size_t i = 0; i < pts.size(); i++) {
    std::cout << "point: " << to_string(pts[i].pos)
              << ", dist = " << grid.getDistance(pts[i].pos) << '\n';
  }

  /* Loop until the user closes the window */
  while (!glfwWindowShouldClose(window)) {
    // reset
    glClearColor(0.f, 0.f, 0.4f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // view control
    computeMatricesFromInputs(projection, view);
    glUniformMatrix4fv(uniV, 1, GL_FALSE, value_ptr(view));
    glUniformMatrix4fv(uniP, 1, GL_FALSE, value_ptr(projection));
    glUniform3fv(uniEyePoint, 1, value_ptr(eyePoint));

    // draw mesh
    glBindVertexArray(mesh.vao);
    glDrawArrays(GL_TRIANGLES, 0, mesh.faces.size() * 3);

    // std::cout << "point: " << to_string(pts[0].pos)
    //           << ", dist = " << grid.getDistance(pts[0].pos) << '\n';

    drawPoints(pts);
    // vec3 start = pts[0].pos;
    // vec3 end =
    //     start + (grid.getGradient(pts[0].pos) *
    //     grid.getDistance(pts[0].pos));
    // drawLine(start, end);
    //
    // pts[0].pos += vec3(0, 0, 0.1);

    for (size_t i = 0; i < pts.size(); i++) {
      vec3 start = pts[i].pos;
      vec3 end =
          start + (grid.getGradient(pts[i].pos) * grid.getDistance(pts[i].pos));
      drawLine(start, end);
    }

    /* Swap front and back buffers */
    glfwSwapBuffers(window);

    /* Poll for and process events */
    glfwPollEvents();
  }

  releaseResource();

  return 0;
}

void initGL() { // Initialise GLFW
  if (!glfwInit()) {
    fprintf(stderr, "Failed to initialize GLFW\n");
    getchar();
    exit(EXIT_FAILURE);
  }

  // without setting GLFW_CONTEXT_VERSION_MAJOR and _MINOR，
  // OpenGL 1.x will be used
  glfwWindowHint(GLFW_SAMPLES, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

  // must be used if OpenGL version >= 3.0
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  // Open a window and create its OpenGL context
  window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "With normal mapping",
                            NULL, NULL);

  if (window == NULL) {
    std::cout << "Failed to open GLFW window." << std::endl;
    glfwTerminate();
    exit(EXIT_FAILURE);
  }

  glfwMakeContextCurrent(window);
  glfwSetKeyCallback(window, keyCallback);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  /* Initialize GLEW */
  // without this, glGenVertexArrays will report ERROR!
  glewExperimental = GL_TRUE;

  if (glewInit() != GLEW_OK) {
    fprintf(stderr, "Failed to initialize GLEW\n");
    getchar();
    glfwTerminate();
    exit(EXIT_FAILURE);
  }

  glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST); // must enable depth test!!

  glPointSize(10);
  glLineWidth(2.0f);
  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
}

void initMatrix() {
  // transform matrix
  uniM = myGetUniformLocation(exeShader, "M");
  uniV = myGetUniformLocation(exeShader, "V");
  uniP = myGetUniformLocation(exeShader, "P");

  model = translate(mat4(1.f), vec3(0.f, 0.f, 0.f));
  view = lookAt(eyePoint,     // eye position
                eyeDirection, // look at
                up            // up
  );

  projection =
      perspective(initialFoV, 1.f * WINDOW_WIDTH / WINDOW_HEIGHT, 0.1f, 100.f);

  glUniformMatrix4fv(uniM, 1, GL_FALSE, value_ptr(model));
  glUniformMatrix4fv(uniV, 1, GL_FALSE, value_ptr(view));
  glUniformMatrix4fv(uniP, 1, GL_FALSE, value_ptr(projection));

  uniEyePoint = myGetUniformLocation(exeShader, "eyePoint");
  glUniform3fv(uniEyePoint, 1, value_ptr(eyePoint));
}

void initLight() { // light
  uniLightColor = myGetUniformLocation(exeShader, "lightColor");
  glUniform3fv(uniLightColor, 1, value_ptr(lightColor));

  uniLightPosition = myGetUniformLocation(exeShader, "lightPosition");
  glUniform3fv(uniLightPosition, 1, value_ptr(lightPosition));

  // uniLightPower = myGetUniformLocation(exeShader, "lightPower");
  // glUniform1f(uniLightPower, lightPower);
}

void initShader() {
  // build shader program
  exeShader = buildShader("./shader/vsPhong.glsl", "./shader/fsPhong.glsl");
  glUseProgram(exeShader);
}

void releaseResource() { glfwTerminate(); }

void computeMatricesFromInputs(mat4 &newProject, mat4 &newView) {
  // glfwGetTime is called only once, the first time this function is called
  static float lastTime = glfwGetTime();

  // Compute time difference between current and last frame
  float currentTime = glfwGetTime();
  float deltaTime = float(currentTime - lastTime);

  // Get mouse position
  double xpos, ypos;
  glfwGetCursorPos(window, &xpos, &ypos);

  // std::cout << xpos << ", " << ypos << '\n';

  // Reset mouse position for next frame
  glfwSetCursorPos(window, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);

  // Compute new orientation
  // The cursor is set to the center of the screen last frame,
  // so (currentCursorPos - center) is the offset of this frame
  horizontalAngle += mouseSpeed * float(xpos - WINDOW_WIDTH / 2.f);
  verticalAngle += mouseSpeed * float(-ypos + WINDOW_HEIGHT / 2.f);

  // Direction : Spherical coordinates to Cartesian coordinates conversion
  vec3 direction =
      vec3(sin(verticalAngle) * cos(horizontalAngle), cos(verticalAngle),
           sin(verticalAngle) * sin(horizontalAngle));

  // Right vector
  vec3 right = vec3(cos(horizontalAngle - 3.14 / 2.f), 0.f,
                    sin(horizontalAngle - 3.14 / 2.f));

  // new up vector
  vec3 newUp = cross(right, direction);

  // Move forward
  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
    eyePoint += direction * deltaTime * speed;
  }
  // Move backward
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
    eyePoint -= direction * deltaTime * speed;
  }
  // Strafe right
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
    eyePoint += right * deltaTime * speed;
  }
  // Strafe left
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
    eyePoint -= right * deltaTime * speed;
  }

  // float FoV = initialFoV;
  newProject =
      perspective(initialFoV, 1.f * WINDOW_WIDTH / WINDOW_HEIGHT, 0.1f, 100.f);
  // Camera matrix
  newView = lookAt(eyePoint, eyePoint + direction, newUp);

  // For the next frame, the "last time" will be "now"
  lastTime = currentTime;
}

// calculate the position of the cell which covers the specified point
vec3 calCellPos(vec3 pt) {
  // change reference frame
  vec3 ptRef = pt - gridOrigin;

  // grid index along each axis of this cell
  int ix = int(floor(ptRef.x / cellSize));
  int iy = int(floor(ptRef.y / cellSize));
  int iz = int(floor(ptRef.z / cellSize));

  // position of this cell
  vec3 posRef = vec3(ix * cellSize, iy * cellSize, iz * cellSize);

  // change reference frame
  vec3 pos = posRef + gridOrigin;

  return pos;
}

void initGrid() {
  grid.origin = gridOrigin;
  grid.cellSize = cellSize;
  grid.nOfCells = nOfCells;

  // ATTENTION: the order of iterating x, y, z relates to
  // the hash of cell index
  // for (size_t iz = 0; iz < nOfCells.z; iz++) {
  //   for (size_t iy = 0; iy < nOfCells.y; iy++) {
  //     for (size_t ix = 0; ix < nOfCells.x; ix++) {
  //       Cell cell;
  //
  //       cell.idx = ivec3(ix, iy, iz);
  //       cell.sd = 9999.f;
  //
  //       cell.pos = vec3(ix, iy, iz) * cellSize + gridOrigin;
  //
  //       grid.cells.push_back(cell);
  //     } // end of iterate x
  //   }   // end of iterate y
  // }     // end of iterate z

  readSdf(grid, "sdfCube.txt");
}

// format: x, y, z, i, j, k, dist
void writeSdf(Grid &gd, const string fileName) {
  ofstream output(fileName);

  for (size_t i = 0; i < gd.cells.size(); i++) {
    Cell &cell = gd.cells[i];

    output << cell.pos.x;
    output << " ";
    output << cell.pos.y;
    output << " ";
    output << cell.pos.z;
    output << " ";
    output << cell.idx.x;
    output << " ";
    output << cell.idx.y;
    output << " ";
    output << cell.idx.z;
    output << " ";
    output << cell.sd;
    output << '\n';
  }

  output.close();
}

// format: x, y, z, i, j, k, dist
void readSdf(Grid &gd, const string fileName) {
  ifstream fin;
  fin.open(fileName.c_str());

  if (!(fin.good())) {
    cout << "failed to open file : " << fileName << std::endl;
  }

  // read file
  while (fin.peek() != EOF) {
    Cell cell;

    fin >> cell.pos.x;
    fin >> cell.pos.y;
    fin >> cell.pos.z;

    fin >> cell.idx.x;
    fin >> cell.idx.y;
    fin >> cell.idx.z;

    fin >> cell.sd;

    gd.cells.push_back(cell);
  } // end read file

  // std::cout << "cells.size()" << gd.cells.size() << '\n';

  fin.close();
}
