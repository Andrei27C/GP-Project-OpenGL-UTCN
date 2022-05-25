#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp> //core glm functionality
#include <glm/gtc/matrix_transform.hpp> //glm extension for generating common transformation matrices
#include <glm/gtc/matrix_inverse.hpp> //glm extension for computing inverse matrices
#include <glm/gtc/type_ptr.hpp> //glm extension for accessing the internal data structure of glm types

#include <cmath>
#include <GL/gl.h>
#include <GL/GLU.h>;

#include "Window.h"
#include "Shader.hpp"
#include "Camera.hpp"
#include "Model3D.hpp"
#include "SkyBox.hpp"

#include <glm/gtc/quaternion.hpp> 
#include <glm/gtx/quaternion.hpp>

#include <iostream>
#include <chrono>
#include <thread>
#include <stdio.h>
#include <Windows.h>
#include <conio.h>

#define min(a,b)  (((a) < (b)) ? (a) : (b))
#define max(a,b)  (((a) > (b)) ? (a) : (b))


// window
gps::Window myWindow;

// matrices
glm::mat4 model;
glm::mat4 view;
glm::mat4 projection;
glm::mat3 normalMatrix;
glm::mat4 axeMatrix;

glm::mat3 lightDirMatrix;
GLuint lightDirMatrixLoc;

// light parameters
glm::vec3 lightDir;
glm::vec3 lightColor;

// shader uniform locations
GLint modelLoc;
GLint viewLoc;
GLint projectionLoc;
GLint normalMatrixLoc;
GLint lightDirLoc;
GLint lightColorLoc;

// camera
gps::Camera myCamera(
    glm::vec3(0.0f, 0.0f, 3.0f),
    glm::vec3(0.0f, 0.0f, -10.0f),
    glm::vec3(0.0f, 1.0f, 0.0f));

GLfloat cameraSpeed = 0.03f;

GLboolean pressedKeys[1024];

// models
gps::Model3D teapot;
gps::Model3D ground;
gps::Model3D axe;
gps::Model3D woodLog1;
gps::Model3D woodLog2;
GLfloat angle;

// shaders
gps::Shader myBasicShader;
gps::Shader lightShader;
gps::Shader depthMapShader;

// skybox
std::vector<const GLchar*> faces;
gps::SkyBox skyBox;
gps::Shader skyBoxShader;

GLenum glCheckError_(const char *file, int line)
{
	GLenum errorCode;
	while ((errorCode = glGetError()) != GL_NO_ERROR) {
		std::string error;
		switch (errorCode) {
            case GL_INVALID_ENUM:
                error = "INVALID_ENUM";
                break;
            case GL_INVALID_VALUE:
                error = "INVALID_VALUE";
                break;
            case GL_INVALID_OPERATION:
                error = "INVALID_OPERATION";
                break;
            case GL_STACK_OVERFLOW:
                error = "STACK_OVERFLOW";
                break;
            case GL_STACK_UNDERFLOW:
                error = "STACK_UNDERFLOW";
                break;
            case GL_OUT_OF_MEMORY:
                error = "OUT_OF_MEMORY";
                break;
            case GL_INVALID_FRAMEBUFFER_OPERATION:
                error = "INVALID_FRAMEBUFFER_OPERATION";
                break;
        }
		std::cout << error << " | " << file << " (" << line << ")" << std::endl;
	}
	return errorCode;
}
#define glCheckError() glCheckError_(__FILE__, __LINE__)

float getDistanceToAxe()
{
	float axeX = -0.740934, axeY = 0.262377, axeZ = 1.806109;
	float cameraX, cameraY, cameraZ;
	float distance = 0;
	myCamera.getCameraPosition(&cameraX, &cameraY, &cameraZ);
	//printf("coord: %f %f %f\n",cameraX, cameraY, cameraZ);
	distance = sqrt((axeX - cameraX) * (axeX - cameraX) + (axeY - cameraY) * (axeY - cameraY) + (axeZ - cameraZ) * (axeZ - cameraZ));
	return distance;
}

int retina_width, retina_height;

void windowResizeCallback(GLFWwindow* window, int width, int height) {
	fprintf(stdout, "Window resized! New width: %d , and height: %d\n", width, height);
	//-TODO
	//glfwGetFramebufferSize(glWindow, &retina_width, &retina_height);
	glfwGetFramebufferSize(window, &width, &height);

	myBasicShader.useShaderProgram();

	glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 1000.0f);

	GLint projLoc = glGetUniformLocation(myBasicShader.shaderProgram, "projection");
	glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

	lightShader.useShaderProgram();

	glUniformMatrix4fv(glGetUniformLocation(lightShader.shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

	glViewport(0, 0, width, height);
}

void keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mode) {
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }

	if (key >= 0 && key < 1024) {
        if (action == GLFW_PRESS) {
            pressedKeys[key] = true;
        } else if (action == GLFW_RELEASE) {
            pressedKeys[key] = false;
        }
    }
}

bool mouse = true;
float lastX = 400, lastY = 300;
float yaw = -90.0f, pitch;

void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    //TODO
	if (mouse)
	{
		lastX = xpos;
		lastY = ypos;
		mouse = false;
	}

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos; 
	lastX = xpos;
	lastY = ypos;

	float sensitivity = 0.2f;
	xoffset *= sensitivity;
	yoffset *= sensitivity;

	yaw += xoffset;
	pitch += yoffset;

	if (pitch > 89.0f)
		pitch = 89.0f;
	if (pitch < -89.0f)
		pitch = -89.0f;

	myCamera.rotate(pitch, yaw);
	view = myCamera.getViewMatrix();
	myBasicShader.useShaderProgram();
	glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
	normalMatrix = glm::mat3(glm::inverseTranspose(view * model));
	
	//myCamera.displayCameraPosition();
}

// initialize faces for skybox
void initSkyBoxFaces()
{
	faces.push_back("textures/skybox/right.jpg");
	faces.push_back("textures/skybox/left.jpg");
	faces.push_back("textures/skybox/top.jpg");
	faces.push_back("textures/skybox/bottom.jpg");
	faces.push_back("textures/skybox/back.jpg");
	faces.push_back("textures/skybox/front.jpg");
}

void initSkyBoxFaces2()
{
	faces.push_back("textures/skybox2/right.tga");
	faces.push_back("textures/skybox2/left.tga");
	faces.push_back("textures/skybox2/top.tga");
	faces.push_back("textures/skybox2/bottom.tga");
	faces.push_back("textures/skybox2/front.tga");
	faces.push_back("textures/skybox2/back.tga");
}

//fog
int fogEnable = 1;
GLint fogEnableLoc;
GLfloat fogDensity = 0.05f;

void processMovement() {
	if (pressedKeys[GLFW_KEY_W]) {
		myCamera.move(gps::MOVE_FORWARD, cameraSpeed);
		//update view matrix
        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        // compute normal matrix for teapot
        normalMatrix = glm::mat3(glm::inverseTranspose(view*model));
	}

	if (pressedKeys[GLFW_KEY_S]) {
		myCamera.move(gps::MOVE_BACKWARD, cameraSpeed);
        //update view matrix
        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        // compute normal matrix for teapot
        normalMatrix = glm::mat3(glm::inverseTranspose(view*model));
	}

	if (pressedKeys[GLFW_KEY_A]) {
		myCamera.move(gps::MOVE_LEFT, cameraSpeed);
        //update view matrix
        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        // compute normal matrix for teapot
        normalMatrix = glm::mat3(glm::inverseTranspose(view*model));
	}

	if (pressedKeys[GLFW_KEY_D]) {
		myCamera.move(gps::MOVE_RIGHT, cameraSpeed);
        //update view matrix
        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        // compute normal matrix for teapot
        normalMatrix = glm::mat3(glm::inverseTranspose(view*model));
	}

    if (pressedKeys[GLFW_KEY_Q]) {
        angle -= 1.0f;
        // update model matrix for teapot
        model = glm::rotate(glm::mat4(1.0f), glm::radians(angle), glm::vec3(0, 1, 0));
        // update normal matrix for teapot
        normalMatrix = glm::mat3(glm::inverseTranspose(view*model));
    }

    if (pressedKeys[GLFW_KEY_E]) {
        angle += 1.0f;
        // update model matrix for teapot
        model = glm::rotate(glm::mat4(1.0f), glm::radians(angle), glm::vec3(0, 1, 0));
        // update normal matrix for teapot
        normalMatrix = glm::mat3(glm::inverseTranspose(view*model));
    }

	// line view
	if (pressedKeys[GLFW_KEY_1]) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

	// point view
	if (pressedKeys[GLFW_KEY_2]) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
	}

	// normal view
	if (pressedKeys[GLFW_KEY_3]) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}

	// start fog
	if (pressedKeys[GLFW_KEY_F]) {

		myBasicShader.useShaderProgram();
		fogEnable = 1;
		fogEnableLoc = glGetUniformLocation(myBasicShader.shaderProgram, "fogEnable");
		glUniform1i(fogEnableLoc, fogEnable);

	}
	///*
	// stop fog
	if (pressedKeys[GLFW_KEY_G]) {
		myBasicShader.useShaderProgram();
		fogEnable = 0;
		fogEnableLoc = glGetUniformLocation(myBasicShader.shaderProgram, "fogEnable");
		glUniform1i(fogEnableLoc, fogEnable);

	}//*/

	// increase the intensity of fog
	if (pressedKeys[GLFW_KEY_H])
	{
		fogDensity = min(fogDensity + 0.01f, 1.0f);
	}

	// decrease the intensity of fog
	if (pressedKeys[GLFW_KEY_J])
	{
		fogDensity = max(fogDensity - 0.01f, 0.0f);
	}
}

void initOpenGLWindow() {
    myWindow.Create(1024, 768, "OpenGL Project Core");
}

void setWindowCallbacks() {
	glfwSetWindowSizeCallback(myWindow.getWindow(), windowResizeCallback);
    glfwSetKeyCallback(myWindow.getWindow(), keyboardCallback);
    glfwSetCursorPosCallback(myWindow.getWindow(), mouseCallback);
}

void initOpenGLState() {
	glClearColor(0.7f, 0.7f, 0.7f, 1.0f);
	glViewport(0, 0, myWindow.getWindowDimensions().width, myWindow.getWindowDimensions().height);
    glEnable(GL_FRAMEBUFFER_SRGB);
	glEnable(GL_DEPTH_TEST); // enable depth-testing
	glDepthFunc(GL_LESS); // depth-testing interprets a smaller value as "closer"
	glEnable(GL_CULL_FACE); // cull face
	glCullFace(GL_BACK); // cull back face
	glFrontFace(GL_CCW); // GL_CCW for counter clock-wise
}

void initModels() {
    teapot.LoadModel("models/teapot/teapot20segUT.obj");
	ground.LoadModel("models/ground/secondtry.obj");
	axe.LoadModel("models/axe/axe.obj");
	woodLog1.LoadModel("models/woodLog1/woodLog1.obj");
	woodLog2.LoadModel("models/woodLog2/woodLog2.obj");
}

void initShaders() {
	myBasicShader.loadShader(
        "shaders/basic.vert",
        "shaders/basic.frag");
}

void initSkyBoxShader()
{
	skyBox.Load(faces);
	skyBoxShader.loadShader("shaders/skyboxShader.vert", "shaders/skyboxShader.frag");
	skyBoxShader.useShaderProgram();

	view = myCamera.getViewMatrix();
	glUniformMatrix4fv(glGetUniformLocation(skyBoxShader.shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
}

void initUniforms() {
	myBasicShader.useShaderProgram();

    // create model matrix for teapot
    model = glm::rotate(glm::mat4(1.0f), glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));
	modelLoc = glGetUniformLocation(myBasicShader.shaderProgram, "model");

	// get view matrix for current camera
	view = myCamera.getViewMatrix();
	viewLoc = glGetUniformLocation(myBasicShader.shaderProgram, "view");
	// send view matrix to shader
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

    // compute normal matrix for teapot
    normalMatrix = glm::mat3(glm::inverseTranspose(view*model));
	normalMatrixLoc = glGetUniformLocation(myBasicShader.shaderProgram, "normalMatrix");

	// create projection matrix
	projection = glm::perspective(glm::radians(45.0f),
                               (float)myWindow.getWindowDimensions().width / (float)myWindow.getWindowDimensions().height,
                               0.1f, 20.0f);
	projectionLoc = glGetUniformLocation(myBasicShader.shaderProgram, "projection");
	// send projection matrix to shader
	glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));	

	//set the light direction (direction towards the light)
	lightDir = glm::vec3(0.0f, 1.0f, 1.0f);
	lightDirLoc = glGetUniformLocation(myBasicShader.shaderProgram, "lightDir");
	// send light dir to shader
	glUniform3fv(lightDirLoc, 1, glm::value_ptr(lightDir));

	//set light color
	lightColor = glm::vec3(1.0f, 1.0f, 1.0f); //white light
	lightColorLoc = glGetUniformLocation(myBasicShader.shaderProgram, "lightColor");
	// send light color to shader
	glUniform3fv(lightColorLoc, 1, glm::value_ptr(lightColor));

}

void renderTeapot(gps::Shader shader) {
    // select active shader program
    shader.useShaderProgram();

	model = glm::translate(model, glm::vec3(0.0f, -1.0f, 0.0f));

    //send teapot model matrix data to shader
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    //send teapot normal matrix data to shader
    glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(normalMatrix));

    // draw teapot
    teapot.Draw(shader);
	model = glm::rotate(glm::mat4(1.0f), glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));
	modelLoc = glGetUniformLocation(myBasicShader.shaderProgram, "model");
}

void renderGround(gps::Shader shader) {
	shader.useShaderProgram();

	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

	glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(normalMatrix));

	ground.Draw(shader);
}

GLfloat axeAngle = 0.0f;
GLfloat woodLogAngle = 0.0f;
float deltaTime = 0.0f;	
float startFrame = 0.0f; 
float currentFrame = glfwGetTime();
int inAxeRange()
{
	float distanceToAxe = getDistanceToAxe();
	//printf("distance: %f\n axeAngle: %f\n", distanceToAxe,axeAngle);

	if (distanceToAxe <= 1.0f)
	{
		if (axeAngle >= 0.52f)
			axeAngle = 0;
		if (woodLogAngle >= 0.52f)
			woodLogAngle = 0.0f;
		currentFrame = glfwGetTime();
		return 1;
	}
	woodLogAngle = 0.0f;
	axeAngle = 0.0f;
	return 0;
}

void renderAxe(gps::Shader shader) {
	// select active shader program
	shader.useShaderProgram();

	//send teapot model matrix data to shader
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

	//send teapot normal matrix data to shader
	glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(normalMatrix));

	// draw teapot
	axe.Draw(shader);
}

void animationAxe()
{
	// select active shader program
	myBasicShader.useShaderProgram();

	model = glm::translate(model, glm::vec3(-0.809835f, 0.180243f, 1.829416f) - glm::vec3(0.0f, 0.0f, 0.0f));
	model = glm::rotate(model, -axeAngle, glm::vec3(0, 0, 1));
	model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f) - glm::vec3(-0.809835f, 0.180243f, 1.829416f));

	glUniformMatrix4fv(glGetUniformLocation(myBasicShader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));

	axe.Draw(myBasicShader);
	axeAngle += 0.005f;
	model = glm::rotate(glm::mat4(1.0f), glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));
	modelLoc = glGetUniformLocation(myBasicShader.shaderProgram, "model");
}

void animationWoodLogs()
{
	//wl1 -0.723645 0.203855 1.805677
	//wl2 -0.731435 0.092108 1.796738

	//WOOD LOG Animation 1
	// select active shader program
	myBasicShader.useShaderProgram();

	//glm::mat4 modelAux = model;
	model = glm::translate(model, glm::vec3(-0.723645f, 0.092108f, 1.805677f) - glm::vec3(0.0f, 0.0f, 0.0f));
	model = glm::rotate(model, woodLogAngle, glm::vec3(1, 0, 0));
	model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f) - glm::vec3(-0.723645f, 0.092108f, 1.805677f));

	glUniformMatrix4fv(glGetUniformLocation(myBasicShader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
	
	woodLog1.Draw(myBasicShader);
	model = glm::rotate(glm::mat4(1.0f), glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));
	modelLoc = glGetUniformLocation(myBasicShader.shaderProgram, "model");

	//WOOD LOG Animation 2
	myBasicShader.useShaderProgram();

	//glm::mat4 modelAux = model;
	model = glm::translate(model, glm::vec3(-0.731435f, 0.092108f, 1.796738f) - glm::vec3(0.0f, 0.0f, 0.0f));
	model = glm::rotate(model, -woodLogAngle, glm::vec3(1, 0, 0));
	model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f) - glm::vec3(-0.731435f, 0.092108f, 1.796738f));

	glUniformMatrix4fv(glGetUniformLocation(myBasicShader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
	
	woodLog2.Draw(myBasicShader);
	model = glm::rotate(glm::mat4(1.0f), glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));
	modelLoc = glGetUniformLocation(myBasicShader.shaderProgram, "model");

	woodLogAngle += 0.02167f;
}

void renderWoodLog1(gps::Shader shader) {
	// select active shader program
	shader.useShaderProgram();

	//send teapot model matrix data to shader
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

	//send teapot normal matrix data to shader
	glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(normalMatrix));

	// draw teapot
	woodLog1.Draw(shader);
}

void renderWoodLog2(gps::Shader shader) {
	// select active shader program
	shader.useShaderProgram();

	//model = glm::rotate(glm::mat4(1.0f), glm::radians(angle), glm::vec3(0, 1, 0));


	//model = glm::translate(model, glm::vec3(0.0f, -0.01f, 0.0f));


	//model = glm::rotate(model, glm::radians(-30.0f), glm::vec3(0, 1, 0));
	//model = glm::scale(model, glm::vec3(0.9f));

	//send teapot model matrix data to shader
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

	//send teapot normal matrix data to shader
	glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(normalMatrix));

	// draw teapot
	woodLog2.Draw(shader);
}

void renderScene() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//render the scene

	// render the teapot
	renderTeapot(myBasicShader);

	//render Ground 
	renderGround(myBasicShader);

	//render Axe and Wood Logs
	if(inAxeRange())
	{
		animationAxe();
	}
	else
		renderAxe(myBasicShader);
	if (axeAngle >= 0.4f)
	{
		animationWoodLogs();
	}
	else {
		renderWoodLog1(myBasicShader);
		renderWoodLog2(myBasicShader);
	}
	

	//view matrix to hsader
	view = myCamera.getViewMatrix();

	//skybox
	skyBox.Draw(skyBoxShader, view, projection);
}

void cleanup() {
    myWindow.Delete();
    //cleanup code for your own data
}

int main(int argc, const char * argv[]) {
	//_getch();
    try {
        initOpenGLWindow();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
		//_getch();
        return EXIT_FAILURE;
    }
	//_getch();
    initOpenGLState();
	initModels();
	initShaders();
	initUniforms();

	//skybox
	initSkyBoxFaces2();
	initSkyBoxShader();

    setWindowCallbacks();

	//glfwSetInputMode(myWindow.getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	glCheckError();
	// application loop
	int asd = 0;
	while (!glfwWindowShouldClose(myWindow.getWindow())) {
        processMovement();
	    renderScene();
		
		glfwPollEvents();
		glfwSwapBuffers(myWindow.getWindow());
		glCheckError();
	}

	cleanup();

    return EXIT_SUCCESS;
}
