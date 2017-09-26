#include "load_save_png.hpp"
#include "GL.hpp"
#include "Meshes.hpp"
#include "Scene.hpp"
#include "read_chunk.hpp"

#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <fstream>

static GLuint compile_shader(GLenum type, std::string const &source);
static GLuint link_program(GLuint vertex_shader, GLuint fragment_shader);


class Balloon {
public:
	enum class State {Healthy, Popping, Gone};
	static Scene::Object* popped;
	static std::vector<Balloon*> ActiveBalloons;

	float radius = 0.5; //balloon rad is approx 1
	Scene::Object* object = nullptr;
	glm::vec3 vel = glm::vec3();
	State state = State::Healthy;
	float elapsed_pop = 0; //only valid when State::Popping. Animate bigger, pop, shrink for black hole effect?

	Balloon(){}

	static Balloon* addBalloon(Scene::Object* object, float radius=1){
		Balloon* balloon = (Balloon*) malloc(sizeof(Balloon));
		balloon->radius = radius;
		balloon->object = object;
		balloon->vel = glm::vec3(0,0,1);

		ActiveBalloons.push_back(balloon);
		return balloon;
	}

	static void step(float elapsed){
		std::vector<Balloon*>::iterator it = ActiveBalloons.begin();
		

		while(it != ActiveBalloons.end()){
			Balloon* balloon = *it;
			glm::vec3* pos = &(balloon->object->transform.position);
			switch(balloon->state){
			case State::Gone:
				//do nothing. Could reset balloon position here if wanted
				break;
			case State::Healthy:
				if(pos->z+elapsed*balloon->vel.z > 3 || pos->z+elapsed*balloon->vel.z < balloon->radius) balloon->vel *= -1;
				*pos += elapsed*balloon->vel;
				break;
			case State::Popping:
				balloon->elapsed_pop += elapsed;
				balloon->object->invisible = true;
				popped->invisible = false;
				popped->transform.position = balloon->object->transform.position;
				if(balloon->elapsed_pop > 1){
					balloon->state = State::Gone;
					balloon->object->invisible = true;
					popped->invisible = true;
					balloon->elapsed_pop = 0;
				}
				break;
			}
			it++;
		}
	}
	void pop(){
		if(state == State::Healthy) state = State::Popping;
	}
	static bool gameOver(){
		for(auto balloon : ActiveBalloons){
			if(balloon->state != State::Gone) return false;
		}
		printf("Game over!\n");
		return true;
	}
};
std::vector<Balloon*> Balloon::ActiveBalloons = std::vector<Balloon*>();
Scene::Object* Balloon::popped = nullptr;

int main(int argc, char **argv) {
	//Configuration:
	struct {
		std::string title = "Game2: Robot Fun Police";
		glm::uvec2 size = glm::uvec2(1280, 960);
	} config;

	struct {
		float base = 0,
		      low = 0,
		      mid = 0,
		      high = 0;
		void add(float& val,float inc){
			static const float pi = 3.14159265f;
			val = fmod(val+inc+2*pi,2*pi);
		}
	} robotState; // list of angles. Should keep them bounded

	//------------  initialization ------------

	//Initialize SDL library:
	SDL_Init(SDL_INIT_VIDEO);

	//Ask for an OpenGL context version 3.3, core profile, enable debug:
	SDL_GL_ResetAttributes();
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

	//create window:
	SDL_Window *window = SDL_CreateWindow(
		config.title.c_str(),
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		config.size.x, config.size.y,
		SDL_WINDOW_OPENGL /*| SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI*/
	);

	if (!window) {
		std::cerr << "Error creating SDL window: " << SDL_GetError() << std::endl;
		return 1;
	}

	//Create OpenGL context:
	SDL_GLContext context = SDL_GL_CreateContext(window);

	if (!context) {
		SDL_DestroyWindow(window);
		std::cerr << "Error creating OpenGL context: " << SDL_GetError() << std::endl;
		return 1;
	}

	#ifdef _WIN32
	//On windows, load OpenGL extensions:
	if (!init_gl_shims()) {
		std::cerr << "ERROR: failed to initialize shims." << std::endl;
		return 1;
	}
	#endif

	//Set VSYNC + Late Swap (prevents crazy FPS):
	if (SDL_GL_SetSwapInterval(-1) != 0) {
		std::cerr << "NOTE: couldn't set vsync + late swap tearing (" << SDL_GetError() << ")." << std::endl;
		if (SDL_GL_SetSwapInterval(1) != 0) {
			std::cerr << "NOTE: couldn't set vsync (" << SDL_GetError() << ")." << std::endl;
		}
	}

	//Hide mouse cursor (note: showing can be useful for debugging):
	//SDL_ShowCursor(SDL_DISABLE);

	//------------ opengl objects / game assets ------------

	//shader program:
	GLuint program = 0;
	GLuint program_Position = 0;
	GLuint program_Normal = 0;
	GLuint program_Color = 0;
	GLuint program_mvp = 0;
	GLuint program_itmv = 0;
	GLuint program_to_light = 0;
	{ //compile shader program:
		GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER,
			"#version 330\n"
			"uniform mat4 mvp;\n"
			"uniform mat3 itmv;\n"
			"in vec4 Position;\n"
			"in vec3 Normal;\n"
			"in vec3 Color;\n"
			"out vec3 normal;\n"
			"out vec3 color;\n"
			"void main() {\n"
			"	gl_Position = mvp * Position;\n"
			"	normal = itmv * Normal;\n"
			"	color = Color;\n"
			"}\n"
		);

		GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER,
			"#version 330\n"
			"uniform vec3 to_light;\n"
			"in vec3 normal;\n"
			"in vec3 color;\n"
			"out vec4 fragColor;\n"
			"void main() {\n"
			"	float light = max(0.0, dot(normalize(normal), to_light));\n"
			"	fragColor = vec4(light*color, 1.0);\n"
			"}\n"
		);

		program = link_program(fragment_shader, vertex_shader);

		//look up attribute locations:
		program_Position = glGetAttribLocation(program, "Position");
		if (program_Position == -1U) throw std::runtime_error("no attribute named Position");
		program_Normal = glGetAttribLocation(program, "Normal");
		if (program_Normal == -1U) throw std::runtime_error("no attribute named Normal");

		program_Color = glGetAttribLocation(program, "Color");
		if (program_Color == -1U) throw std::runtime_error("no attribute named Color");

		//look up uniform locations:
		program_mvp = glGetUniformLocation(program, "mvp");
		if (program_mvp == -1U) throw std::runtime_error("no uniform named mvp");
		program_itmv = glGetUniformLocation(program, "itmv");
		if (program_itmv == -1U) throw std::runtime_error("no uniform named itmv");

		program_to_light = glGetUniformLocation(program, "to_light");
		if (program_to_light == -1U) throw std::runtime_error("no uniform named to_light");
	}

	//------------ meshes ------------

	Meshes meshes;

	{ //add meshes to database:
		Meshes::Attributes attributes;
		attributes.Position = program_Position;
		attributes.Normal = program_Normal;
		attributes.Color = program_Color;

		meshes.load("meshes.blob", attributes);
	}
	
	//------------ scene ------------

	Scene scene;
	//set up camera parameters based on window:
	scene.camera.fovy = glm::radians(60.0f);
	scene.camera.aspect = float(config.size.x) / float(config.size.y);
	scene.camera.near = 0.01f;
	//(transform will be handled in the update function below)

	//add some objects from the mesh library:
	auto add_object = [&](std::string const &name, glm::vec3 const &position, glm::quat const &rotation, glm::vec3 const &scale) -> Scene::Object & {
		Mesh const &mesh = meshes.get(name);
		scene.objects.emplace_back();
		Scene::Object &object = scene.objects.back();
		object.transform.position = position;
		object.transform.rotation = rotation;
		object.transform.scale = scale;
		object.vao = mesh.vao;
		object.start = mesh.start;
		object.count = mesh.count;
		object.program = program;
		object.program_mvp = program_mvp;
		object.program_itmv = program_itmv;
		object.name = name;
		return object;
	};

	Scene::Transform *stand,*base,*link1,*link2,*link3,*tip;
	{ //read objects to add from "scene.blob":
		std::ifstream file("scene.blob", std::ios::binary);

		std::vector< char > strings;
		//read strings chunk:
		read_chunk(file, "str0", &strings);

		{ //read scene chunk, add meshes to scene:
			struct SceneEntry {
				uint32_t name_begin, name_end;
				glm::vec3 position;
				glm::quat rotation;
				glm::vec3 scale;
			};
			static_assert(sizeof(SceneEntry) == 48, "Scene entry should be packed");

			std::vector< SceneEntry > data;
			read_chunk(file, "scn0", &data);

			for (auto const &entry : data) {
				if (!(entry.name_begin <= entry.name_end && entry.name_end <= strings.size())) {
					throw std::runtime_error("index entry has out-of-range name begin/end");
				}
				std::string name(&strings[0] + entry.name_begin, &strings[0] + entry.name_end);
				add_object(name, entry.position, entry.rotation, entry.scale);
				if(name.substr(0,7) == "Balloon"){
					Balloon::addBalloon(&scene.objects.back());
				}
			}
		}
		//balloon popping
		add_object(std::string("Balloon1-Pop"), glm::vec3(0,0,0), glm::quat(0,0,0,0), glm::vec3(1,1,1));
		Balloon::popped = &scene.objects.back();
		Balloon::popped->invisible = true;
		

		{//setup hierarchy
			stand=base=link1=link2=link3=tip=nullptr;
			for (auto & obj : scene.objects){
				if(obj.name == std::string("Stand")) stand = &obj.transform;
				if(obj.name == std::string("Base")) base = &obj.transform;
				if(obj.name == std::string("Link1")) link1 = &obj.transform;
				if(obj.name == std::string("Link2")) link2 = &obj.transform;
				if(obj.name == std::string("Link3")) link3 = &obj.transform;
				if(obj.name == std::string("Tip")) tip = &obj.transform;
			}
			tip->set_parent(link3);tip->position -= link3->position;
			link3->set_parent(link2);link3->position -= link2->position;
			link2->set_parent(link1);link2->position -= link1->position;
			link1->set_parent(base);link1->position -= base->position;
			base->set_parent(stand);base->position -= stand->position;
		}
	}

	glm::vec2 mouse = glm::vec2(0.0f, 0.0f); //mouse position in [-1,1]x[-1,1] coordinates

	struct {
		float radius = 8.0f;
		float elevation = 0.0f;
		float azimuth = 0.0f;
		glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f);
	} camera;
		
	//------------ game loop ------------

	bool should_quit = false;
	while (true) {
		//handle events
		static SDL_Event evt;
		while (SDL_PollEvent(&evt) == 1) {
			if(evt.type == SDL_KEYDOWN){
				switch(evt.key.keysym.sym){
				case SDLK_ESCAPE:
				case SDLK_q:
					should_quit = true;
					break;
				case SDLK_a:
					robotState.add(robotState.base,0.1f);
					break;
				case SDLK_s:
					robotState.add(robotState.base,-0.1f);
					break;
				case SDLK_z:
					robotState.add(robotState.low,0.1f);
					break;
				case SDLK_x:
					robotState.add(robotState.low,-0.1f);
					break;
				case SDLK_w:
					robotState.add(robotState.mid,0.1f);
					break;
				case SDLK_e:
					robotState.add(robotState.mid,-0.1f);
					break;
				case SDLK_d:
					robotState.add(robotState.high,0.1f);
					break;
				case SDLK_c:
					robotState.add(robotState.high,-0.1f);
					break;
				case SDLK_TAB: //zoom out
					camera.radius ++;
					break;
				case SDLK_LSHIFT: //zoom in
					camera.radius --;
					break;
				default:
					break;
				}
			}
			if (evt.type == SDL_MOUSEMOTION) {
				glm::vec2 old_mouse = mouse;
				mouse.x = (evt.motion.x + 0.5f) / float(config.size.x) * 2.0f - 1.0f;
				mouse.y = (evt.motion.y + 0.5f) / float(config.size.y) *-2.0f + 1.0f;
				if (evt.motion.state & SDL_BUTTON(SDL_BUTTON_LEFT)) {
					camera.elevation += -2.0f * (mouse.y - old_mouse.y);
					camera.azimuth += -2.0f * (mouse.x - old_mouse.x);
				}
			} else if (evt.type == SDL_MOUSEBUTTONDOWN) {
			} else if (evt.type == SDL_QUIT) {
				should_quit = true;
				break;
			}
		}
		if (should_quit) break;


		//update timers
		auto current_time = std::chrono::high_resolution_clock::now();
		static auto previous_time = current_time;
		float elapsed = std::chrono::duration< float >(current_time - previous_time).count();
		previous_time = current_time;
		static float fulltime = 0;
		fulltime += elapsed;

		{ //update game state
			//manage balloons
			Balloon::step(elapsed);

			//update robot pos based on rotations:
			base->rotation = glm::angleAxis(robotState.base,glm::vec3(0,0,1));
			link1->rotation = glm::angleAxis(robotState.low,glm::vec3(1,0,0));
			link2->rotation = glm::angleAxis(robotState.mid,glm::vec3(1,0,0));
			link3->rotation = glm::angleAxis(robotState.high,glm::vec3(1,0,0));

			//manage collisions
			glm::vec4 tipposh = tip->make_local_to_world()*glm::vec4(tip->position,1);
			glm::vec3 tippos = glm::vec3(tipposh.x,tipposh.y,tipposh.z)/tipposh.w;
			for(Balloon* balloon : Balloon::ActiveBalloons){
				if(glm::length(balloon->object->transform.position - tippos) < balloon->radius) balloon->pop();
			}

			if(Balloon::gameOver()){
				static float delay = 0;
				static float endtime = fulltime;
				delay += elapsed;
				if(delay > 2){
					printf("your total time was %.2f!\n",endtime);
					should_quit = true;
				}
			}


			//camera
			scene.camera.transform.position = camera.radius * glm::vec3(
				std::cos(camera.elevation) * std::cos(camera.azimuth),
				std::cos(camera.elevation) * std::sin(camera.azimuth),
				std::sin(camera.elevation)) + camera.target;

			glm::vec3 out = -glm::normalize(camera.target - scene.camera.transform.position);
			glm::vec3 up = glm::vec3(0.0f, 0.0f, 1.0f);
			up = glm::normalize(up - glm::dot(up, out) * out);
			glm::vec3 right = glm::cross(up, out);
			
			scene.camera.transform.rotation = glm::quat_cast(
				glm::mat3(right, up, out)
			);
			scene.camera.transform.scale = glm::vec3(1.0f, 1.0f, 1.0f);
		}

		//draw output
		glClearColor(0.5, 0.5, 0.5, 0.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


		{ //draw game state
			glUseProgram(program);
			glUniform3fv(program_to_light, 1, glm::value_ptr(glm::normalize(glm::vec3(0.0f, 1.0f, 10.0f))));
			scene.render();
		}


		SDL_GL_SwapWindow(window);
	}


	//------------  teardown ------------

	SDL_GL_DeleteContext(context);
	context = 0;

	SDL_DestroyWindow(window);
	window = NULL;

	return 0;
}



static GLuint compile_shader(GLenum type, std::string const &source) {
	GLuint shader = glCreateShader(type);
	GLchar const *str = source.c_str();
	GLint length = source.size();
	glShaderSource(shader, 1, &str, &length);
	glCompileShader(shader);
	GLint compile_status = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
	if (compile_status != GL_TRUE) {
		std::cerr << "Failed to compile shader." << std::endl;
		GLint info_log_length = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_log_length);
		std::vector< GLchar > info_log(info_log_length, 0);
		GLsizei length = 0;
		glGetShaderInfoLog(shader, info_log.size(), &length, &info_log[0]);
		std::cerr << "Info log: " << std::string(info_log.begin(), info_log.begin() + length);
		glDeleteShader(shader);
		throw std::runtime_error("Failed to compile shader.");
	}
	return shader;
}

static GLuint link_program(GLuint fragment_shader, GLuint vertex_shader) {
	GLuint program = glCreateProgram();
	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glLinkProgram(program);
	GLint link_status = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &link_status);
	if (link_status != GL_TRUE) {
		std::cerr << "Failed to link shader program." << std::endl;
		GLint info_log_length = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_length);
		std::vector< GLchar > info_log(info_log_length, 0);
		GLsizei length = 0;
		glGetProgramInfoLog(program, info_log.size(), &length, &info_log[0]);
		std::cerr << "Info log: " << std::string(info_log.begin(), info_log.begin() + length);
		throw std::runtime_error("Failed to link program");
	}
	return program;
}
