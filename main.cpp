#include "../Externals/Include/Include.h"

#define MENU_TIMER_START 1
#define MENU_TIMER_STOP 2
#define MENU_EXIT 3
#define Shader_None 4
#define Shader_Abstraction 5
#define Shader_LaplacianFilter 6
#define Shader_SharpnessFilter 7
#define Shader_Pixelation 8
#define Shader_Fish_eyedistortion 9
#define Shader_Red_BlueStereo 10
#define Shader_Bloom_Effect 11
#define Shader_halftoning 12
#define Shader_NightVision 13
#define Shader_Frosted_Glass 14
#define Shader_Swirl 15

int shader_index = -1;
float elapsedTime = glutGet(GLUT_ELAPSED_TIME)*5.0;

#define _CRT_SECURE_NO_WARNINGS
GLubyte timer_cnt = 0;
bool timer_enabled = true;
unsigned int timer_speed = 16;
const GLfloat Pi = 3.1415926536f;
GLfloat tick;

using namespace glm;
using namespace std;

mat4 view;
mat4 projection;
GLint um4p;
GLint um4mv;
const aiScene *scene;
GLint Shader_Loc;
GLint Time;

static float c = Pi / 180.0f;
static int du = 90, oldmy = -1, oldmx = -1;
static float r = 10.0f, h = 4.0f;
vec3 eye = vec3(0.0f, 1.0f, 0.0f);
vec3 center = vec3(r*cos(-c * du), 5 * h, r*sin(-c * du)) + eye;
vec3 camera_move;
GLuint program;

GLuint			program2;
GLuint          window_vao;
GLuint			vertex_shader;
GLuint			fragment_shader;
GLuint			window_buffer;

// FBO parameter
GLuint			FBO;
GLuint			depthRBO;
GLuint			FBODataTexture;
//GLuint			FBODataTexture2;

typedef struct Shape
{
	GLuint vao;
	GLuint vbo_position;
	GLuint vbo_normal;
	GLuint vbo_texcoord;
	GLuint ibo;
	int drawCount;
	int materialID;
}Shape;

typedef struct Material
{
	GLuint diffuse_tex;
}Material;

vector<Shape> sp_shapes;
vector<Material> sp_materials;

void My_Reshape(int width, int height);
static const GLfloat window_positions[] =
{
	1.0f,-1.0f,1.0f,0.0f,
	-1.0f,-1.0f,0.0f,0.0f,
	-1.0f,1.0f,0.0f,1.0f,
	1.0f,1.0f,1.0f,1.0f
};

char** loadShaderSource(const char* file)
{
    FILE* fp = fopen(file, "rb");
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *src = new char[sz + 1];
    fread(src, sizeof(char), sz, fp);
    src[sz] = '\0';
    char **srcp = new char*[1];
    srcp[0] = src;
    return srcp;
}

void freeShaderSource(char** srcp)
{
    delete[] srcp[0];
    delete[] srcp;
}

// define a simple data structure for storing texture image raw data
typedef struct _TextureData
{
    _TextureData(void) :
        width(0),
        height(0),
        data(0)
    {
    }

    int width;
    int height;
    unsigned char* data;
} TextureData;

// load a png image and return a TextureData structure with raw data
// not limited to png format. works with any image format that is RGBA-32bit
TextureData loadPNG(const char* const pngFilepath)
{
    TextureData texture;
    int components;

    // load the texture with stb image, force RGBA (4 components required)
    stbi_uc *data = stbi_load(pngFilepath, &texture.width, &texture.height, &components, 4);

    // is the image successfully loaded?
    if (data != NULL)
    {
        // copy the raw data
        size_t dataSize = texture.width * texture.height * 4 * sizeof(unsigned char);
        texture.data = new unsigned char[dataSize];
        memcpy(texture.data, data, dataSize);

        // mirror the image vertically to comply with OpenGL convention
        for (size_t i = 0; i < texture.width; ++i)
        {
            for (size_t j = 0; j < texture.height / 2; ++j)
            {
                for (size_t k = 0; k < 4; ++k)
                {
                    size_t coord1 = (j * texture.width + i) * 4 + k;
                    size_t coord2 = ((texture.height - j - 1) * texture.width + i) * 4 + k;
                    std::swap(texture.data[coord1], texture.data[coord2]);
                }
            }
        }

        // release the loaded image
        stbi_image_free(data);
    }

    return texture;
}

void My_LoadModels()
{
	for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
		aiMaterial *material = scene->mMaterials[i];
		Material mtr;
		aiString texturePath;
		if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == aiReturn_SUCCESS) {
			// load width, height and data from texturePath.C_Str();
			int width, height;
			TextureData TD = loadPNG(texturePath.C_Str());
			width = TD.width;
			height = TD.height;
			unsigned char *data = TD.data;

			glGenTextures(1, &mtr.diffuse_tex);
			glBindTexture(GL_TEXTURE_2D, mtr.diffuse_tex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			glGenerateMipmap(GL_TEXTURE_2D);

		}
		// save materialÅc
		sp_materials.push_back(mtr);
	}

	for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
		aiMesh *mesh = scene->mMeshes[i];
		Shape shape;
		glGenVertexArrays(1, &shape.vao);
		glBindVertexArray(shape.vao);
		//create 3 vbos to hold data
		float *vertices = new float[mesh->mNumVertices * 3];
		float *texCoords = new float[mesh->mNumVertices * 2];
		float *normals = new float[mesh->mNumVertices * 3];
		for (unsigned int v = 0; v < mesh->mNumVertices; ++v) {
			vertices[v * 3] = mesh->mVertices[v][0];
			vertices[v * 3 + 1] = mesh->mVertices[v][1];
			vertices[v * 3 + 2] = mesh->mVertices[v][2];
			texCoords[v * 2] = mesh->mTextureCoords[0][v][0];
			texCoords[v * 2 + 1] = mesh->mTextureCoords[0][v][1];
			normals[v * 3] = mesh->mNormals[v][0];
			normals[v * 3 + 1] = mesh->mNormals[v][1];
			normals[v * 3 + 2] = mesh->mNormals[v][2];
		
		}

		glGenBuffers(1, &shape.vbo_position);
		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_position);
		glBufferData(GL_ARRAY_BUFFER, mesh->mNumVertices * sizeof(float) * 3, vertices, GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glGenBuffers(1, &shape.vbo_texcoord);
		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_texcoord);
		glBufferData(GL_ARRAY_BUFFER, mesh->mNumVertices * sizeof(float) * 2, texCoords, GL_STATIC_DRAW);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);

		glGenBuffers(1, &shape.vbo_normal);
		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_normal);
		glBufferData(GL_ARRAY_BUFFER, mesh->mNumVertices * sizeof(float) * 3, normals, GL_STATIC_DRAW);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);

		delete[] vertices;
		delete[] texCoords;
		delete[] normals;

		// create 1 ibo to hold data
		unsigned int *indices = new unsigned int[mesh->mNumFaces * 3];
		for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
			indices[f * 3] = mesh->mFaces[f].mIndices[0];
			indices[f * 3 + 1] = mesh->mFaces[f].mIndices[1];
			indices[f * 3 + 2] = mesh->mFaces[f].mIndices[2];
		}

		glGenBuffers(1, &shape.ibo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shape.ibo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->mNumFaces * sizeof(unsigned int) * 3, indices , GL_STATIC_DRAW);
		
		delete[] indices;

		shape.materialID = mesh->mMaterialIndex;
		shape.drawCount = mesh->mNumFaces * 3;

		//save shape
		sp_shapes.push_back(shape);
	}
	aiReleaseImport(scene);
}

void My_Init()
{
	printf("%x", GL_DRAW_FRAMEBUFFER);
	printf("%x", GL_FRAMEBUFFER);

    glClearColor(1.0f, 1.6f, 1.0f, 1.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	
	scene = aiImportFile("sponza.obj", aiProcessPreset_TargetRealtime_MaxQuality);
	program = glCreateProgram();
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	char** vertexShaderSource = loadShaderSource("vertex.vs.glsl");
	char** fragmentShaderSource = loadShaderSource("fragment.fs.glsl");
	glShaderSource(vertexShader, 1, vertexShaderSource, NULL);
	glShaderSource(fragmentShader, 1, fragmentShaderSource, NULL);
	freeShaderSource(vertexShaderSource);
	freeShaderSource(fragmentShaderSource);
	glCompileShader(vertexShader);
	glCompileShader(fragmentShader);
	shaderLog(vertexShader);
	shaderLog(fragmentShader);
	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);
	glLinkProgram(program);
	um4mv = glGetUniformLocation(program, "um4mv");
	um4p = glGetUniformLocation(program, "um4p");
	glUseProgram(program);

	My_LoadModels();

	program2 = glCreateProgram();

	char** FB_vertexShaderSource = loadShaderSource("FB_vertex.vs.glsl");
	char** FB_fragmentShaderSource = loadShaderSource("FB_fragment.fs.glsl");

	GLuint vs2 = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vs2, 1, FB_vertexShaderSource, NULL);
	freeShaderSource(FB_vertexShaderSource);
	glCompileShader(vs2);
	shaderLog(vs2);

	GLuint fs2 = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fs2, 1, FB_fragmentShaderSource, NULL);
	freeShaderSource(FB_fragmentShaderSource);
	glCompileShader(fs2);
	shaderLog(fs2);

	glAttachShader(program2, vs2);
	glAttachShader(program2, fs2);
	glLinkProgram(program2);


	glGenVertexArrays(1, &window_vao);
	glBindVertexArray(window_vao);

	glGenBuffers(1, &window_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, window_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(window_positions), window_positions, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT) * 4, 0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT) * 4, (const GLvoid*)(sizeof(GL_FLOAT) * 2));

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	Shader_Loc = glGetUniformLocation(program2, "shader_index");
	Time = glGetUniformLocation(program2, "elapsedTime");

	glGenFramebuffers(1, &FBO);

	My_Reshape(600, 600);
	//////////////////////////////////////////////////////////////////////////
	/////////Create RBO and Render Texture in Reshape Function////////////////
	//////////////////////////////////////////////////////////////////////////

}

void My_Display()
{
	// (1) Bind the framebuffer object correctly
	// (2) Draw the buffer with color
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FBO);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(program);

	static const GLfloat white[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	static const GLfloat one = 1.0f;

	// TODO :
	// (1) Clear the color buffer (GL_COLOR) with the color of white
	// (2) Clear the depth buffer (GL_DEPTH) with value one 
	glClearBufferfv(GL_COLOR, 0, white);
	glClearBufferfv(GL_DEPTH, 0, &one);

	// Draw the Scene
	tick = glutGet(GLUT_ELAPSED_TIME)*5.0;
	eye = vec3(0.0f, 1.0f, 0.0f) + camera_move;
	center = vec3(r*cos(c*du), 5 * h, r*sin(c*du)) + camera_move;
	view = lookAt(eye, center, vec3(0.0f, 1.0f, 0.0f));
	glUniformMatrix4fv(um4mv, 1, GL_FALSE, value_ptr(view));
	glUniformMatrix4fv(um4p, 1, GL_FALSE, value_ptr(projection));

	
	for (unsigned int i = 0; i < sp_shapes.size(); ++i) {
		glBindVertexArray(sp_shapes[i].vao);
		int materialID = sp_shapes[i].materialID;
		glBindTexture(GL_TEXTURE_2D, sp_materials[materialID].diffuse_tex);
		
		glDrawElements(GL_TRIANGLES, sp_shapes[i].drawCount, GL_UNSIGNED_INT, 0);
	}
	   	 
	// Re-bind the framebuffer and clear it 
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(1.0f, 0.0f, 0.0f, 1.0f);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, FBODataTexture);
	//glActiveTexture(GL_TEXTURE1);
	//glBindTexture(GL_TEXTURE_2D, FBODataTexture2);

	// (1) Bind the vao we want to render
	// (2) Use the correct shader program
	glBindVertexArray(window_vao);
	glUseProgram(program2);
	
	glUniform1i(Shader_Loc, shader_index);
	glUniform1f(Time, elapsedTime);

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);


    glutSwapBuffers();
}

void My_Reshape(int width, int height)
{
	glViewport(0, 0, width, height);
	float viewportAspect = (float)width / (float)height;
	projection = perspective(radians(60.0f), viewportAspect, 0.1f, 1000.0f);
	view = lookAt(vec3(r*cos(c*du), 5 * h, r*sin(c*du)), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));

	// If the windows is reshaped, we need to reset some settings of framebuffer
	glDeleteRenderbuffers(1, &depthRBO);
	glDeleteTextures(1, &FBODataTexture);
	glGenRenderbuffers(1, &depthRBO);
	glBindRenderbuffer(GL_RENDERBUFFER, depthRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32, width, height);

	// (1) Generate a texture for FBO
	// (2) Bind it so that we can specify the format of the textrue
	glGenTextures(1, &FBODataTexture);
	glBindTexture(GL_TEXTURE_2D, FBODataTexture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// (1) Bind the framebuffer with first parameter "GL_DRAW_FRAMEBUFFER" 
	// (2) Attach a renderbuffer object to a framebuffer object
	// (3) Attach a texture image to a framebuffer object
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FBO);
	glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRBO);
	glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, FBODataTexture, 0);


}

void My_Timer(int val)
{
	glutPostRedisplay();
	timer_cnt++;
	if (timer_enabled)
	{
		glutTimerFunc(timer_speed, My_Timer, val);
	}
}

void My_Mouse(int button, int state, int x, int y)
{
	if(state == GLUT_DOWN)
	{
		oldmx = x, oldmy = y;
		printf("Mouse %d is pressed at (%d, %d)\n", button, x, y);
	}
	else if(state == GLUT_UP)
	{
		printf("Mouse %d is released at (%d, %d)\n", button, x, y);
	}
}

void My_Mouse_Drag(int x, int y)
{
	//printf("%d\n",du);  
	du += x - oldmx;
	h += 0.03f*(oldmy - y);
	oldmx = x, oldmy = y;
}

void My_Keyboard(unsigned char key, int x, int y)
{
	printf("Key %c is pressed at (%d, %d)\n", key, x, y);
	if (key == 'w') {
		camera_move += (center - eye) / 2.0f;
	}
	else if (key == 's') {
		camera_move += (eye - center) / 2.0f;
	}
	else if (key == 'd') {
		camera_move += cross((center - eye), vec3(0.0f, 1.0f, 0.0f)) / 2.0f;
	}
	else if (key == 'a') {
		camera_move += cross(vec3(0.0f, 1.0f, 0.0f), (center - eye)) / 2.0f;
	}
	else if (key == 'z') {
		camera_move += vec3(0.0f, 2.5f, 0.0f);
	}
	else if (key == 'x') {
		camera_move += vec3(0.0f, -2.5f, 0.0f);
	}
	
	
}

void My_SpecialKeys(int key, int x, int y)
{
	switch(key)
	{
	case GLUT_KEY_F1:
		printf("F1 is pressed at (%d, %d)\n", x, y);

		break;
	case GLUT_KEY_PAGE_UP:
		printf("Page up is pressed at (%d, %d)\n", x, y);
		break;
	case GLUT_KEY_LEFT:
		printf("Left arrow is pressed at (%d, %d)\n", x, y);
		break;
	default:
		printf("Other special key is pressed at (%d, %d)\n", x, y);
		break;
	}
}

void My_Menu(int id)
{
	switch(id)
	{
	case MENU_TIMER_START:
		if(!timer_enabled)
		{
			timer_enabled = true;
			glutTimerFunc(timer_speed, My_Timer, 0);
		}
		break;
	case MENU_TIMER_STOP:
		timer_enabled = false;
		break;
	case MENU_EXIT:
		exit(0);
		break;
	case Shader_None:
		shader_index = 0;
		break;
	case Shader_Abstraction:
		shader_index = 1;
		break;
	case Shader_LaplacianFilter:
		shader_index = 2;
		break;
	case Shader_SharpnessFilter:
		shader_index = 3;
		break;
	case Shader_Pixelation:
		shader_index = 4;
		break;
	case Shader_Fish_eyedistortion:
		shader_index = 5;
		break;
	case Shader_Red_BlueStereo:
		shader_index = 6;
		break;
	case Shader_Bloom_Effect:
		shader_index = 7;
		break;
	case Shader_halftoning:
		shader_index = 8;
		break;
	case Shader_NightVision:
		shader_index = 9;
		break;
	case Shader_Frosted_Glass:
		shader_index = 10;
		break;
	case Shader_Swirl:
		shader_index = 11;
		break;	
	default:
		break;
	}
}


int main(int argc, char *argv[])
{
#ifdef __APPLE__
    // Change working directory to source code path
    chdir(__FILEPATH__("/../Assets/"));
#endif
	// Initialize GLUT and GLEW, then create a window.
	////////////////////
	glutInit(&argc, argv);
#ifdef _MSC_VER
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
#else
    glutInitDisplayMode(GLUT_3_2_CORE_PROFILE | GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
#endif
	glutInitWindowPosition(100, 100);
	glutInitWindowSize(600, 600);
	glutCreateWindow("AS2_Framework"); // You cannot use OpenGL functions before this line; The OpenGL context must be created first by glutCreateWindow()!
#ifdef _MSC_VER
	glewInit();
#endif
    glPrintContextInfo();
	My_Init();

	// Create a menu and bind it to mouse right button.
	int menu_main = glutCreateMenu(My_Menu);
	int menu_timer = glutCreateMenu(My_Menu);
	int shader = glutCreateMenu(My_Menu);

	glutSetMenu(menu_main);
	glutAddSubMenu("Timer", menu_timer);
	glutAddSubMenu("Shader", shader);

	glutAddMenuEntry("Exit", MENU_EXIT);

	glutSetMenu(menu_timer);
	glutAddMenuEntry("Start", MENU_TIMER_START);
	glutAddMenuEntry("Stop", MENU_TIMER_STOP);

	glutSetMenu(shader);
	glutAddMenuEntry("NoShader", Shader_None);
	glutAddMenuEntry("Abstraction", Shader_Abstraction);
	glutAddMenuEntry("LaplacianFilter", Shader_LaplacianFilter);
	glutAddMenuEntry("SharpnessFilter", Shader_SharpnessFilter);
	glutAddMenuEntry("Pixelation", Shader_Pixelation);
	glutAddMenuEntry("Fish-eyedistortion", Shader_Fish_eyedistortion);
	glutAddMenuEntry("Red-BlueStereo", Shader_Red_BlueStereo);
	glutAddMenuEntry("Bloom_Effect", Shader_Bloom_Effect);
	glutAddMenuEntry("halftoning", Shader_halftoning);
	glutAddMenuEntry("NightVision", Shader_NightVision);
	glutAddMenuEntry("Frosted_Glass", Shader_Frosted_Glass);
	glutAddMenuEntry("Swirl", Shader_Swirl);
	

	glutSetMenu(menu_main);
	glutAttachMenu(GLUT_RIGHT_BUTTON);

	// Register GLUT callback functions.
	glutDisplayFunc(My_Display);
	glutReshapeFunc(My_Reshape);
	glutMouseFunc(My_Mouse);
	glutMotionFunc(My_Mouse_Drag);
	glutKeyboardFunc(My_Keyboard);
	glutSpecialFunc(My_SpecialKeys);
	glutTimerFunc(timer_speed, My_Timer, 0); 

	// Enter main event loop.
	glutMainLoop();

	return 0;
}
