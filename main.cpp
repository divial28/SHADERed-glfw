#ifdef _WIN32
#include <windows.h>
#endif

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <SHADERed/EditorEngine.h>
#include <SHADERed/Objects/CommandLineOptionParser.h>
#include <SHADERed/Objects/ShaderCompiler.h>
#include <SHADERed/Objects/Logger.h>
#include <SHADERed/Objects/Settings.h>
#include <SHADERed/Objects/Export/ExportCPP.h>
#include <glslang/Public/ShaderLang.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include <string>

#include <misc/stb_image.h>
#include <misc/stb_image_write.h>

#if defined(__linux__) || defined(__unix__)
#include <libgen.h>
#include <unistd.h>
#endif

#if defined(_WIN32)
extern "C" {
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

#if defined(NDEBUG) && defined(_WIN32)
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#endif

// SDL defines main
#undef main

void SetIcon(GLFWwindow* wnd);
void SetDpiAware();

int main(int argc, char* argv[])
{
	srand(time(NULL));
	
	std::error_code fsError;
	std::filesystem::path cmdDir = std::filesystem::current_path();

	if (argc > 0) {
		if (std::filesystem::exists(std::filesystem::path(argv[0]).parent_path())) {
			std::filesystem::current_path(std::filesystem::path(argv[0]).parent_path(), fsError);

			ed::Logger::Get().Log("Setting current_path to " + std::filesystem::current_path().generic_string());
		}
	}

	// start glslang process
	bool glslangInit = glslang::InitializeProcess();
	ed::Logger::Get().Log("Initializing glslang...");

	if (glslangInit)
		ed::Logger::Get().Log("Finished glslang initialization");
	else
		ed::Logger::Get().Log("Failed to initialize glslang", true);

	ed::CommandLineOptionParser coptsParser;
	coptsParser.Parse(cmdDir, argc - 1, argv + 1);
	coptsParser.Execute();

	if (!coptsParser.LaunchUI)
		return 0;

#if defined(__linux__) || defined(__unix__)
	bool linuxUseHomeDir = false;

	// currently the only supported argument is a path to set the working directory... dont do this check if user wants to explicitly set the working directory,
	// TODO: if more arguments get added, use different methods to check if working directory is being set explicitly
	{
		char result[PATH_MAX + 1];
		ssize_t readlinkRes = readlink("/proc/self/exe", result, PATH_MAX);
		std::string exePath = "";
		if (readlinkRes > 0) {
			result[readlinkRes] = '\0';
			exePath = std::string(dirname(result));
		}
		
		std::vector<std::string> toCheck = {
			"/../share/SHADERed",
			"/../share/shadered"
			// TODO: maybe more paths here?
		};

		for (const auto& wrkpath : toCheck) {
			if (std::filesystem::exists(exePath + wrkpath, fsError)) {
				linuxUseHomeDir = true;
				std::filesystem::current_path(exePath + wrkpath, fsError);
				ed::Logger::Get().Log("Setting current_path to " + std::filesystem::current_path().generic_string());
				break;
			}
		}

		if (access(exePath.c_str(), W_OK) != 0) 
			linuxUseHomeDir = true;		
	}

	if (linuxUseHomeDir) {
		const char *homedir = getenv("XDG_DATA_HOME");
		std::string homedirSuffix = "";
		if (homedir == NULL) {
			homedir = getenv("HOME");
			homedirSuffix = "/.local/share";
		}
		
		if (homedir != NULL) {
			ed::Settings::Instance().LinuxHomeDirectory = std::string(homedir) + homedirSuffix + "/shadered/";

			if (!std::filesystem::exists(ed::Settings::Instance().LinuxHomeDirectory, fsError))
				std::filesystem::create_directory(ed::Settings::Instance().LinuxHomeDirectory, fsError);
			if (!std::filesystem::exists(ed::Settings::Instance().ConvertPath("data"), fsError))
				std::filesystem::create_directory(ed::Settings::Instance().ConvertPath("data"), fsError);
			if (!std::filesystem::exists(ed::Settings::Instance().ConvertPath("themes"), fsError))
				std::filesystem::create_directory(ed::Settings::Instance().ConvertPath("themes"), fsError);
			if (!std::filesystem::exists(ed::Settings::Instance().ConvertPath("plugins"), fsError))
				std::filesystem::create_directory(ed::Settings::Instance().ConvertPath("plugins"), fsError);
		}
	}
#endif

	// create data directory on startup
	if (!std::filesystem::exists(ed::Settings::Instance().ConvertPath("data/"), fsError))
		std::filesystem::create_directory(ed::Settings::Instance().ConvertPath("data/"), fsError);

	// create temp directory
	if (!std::filesystem::exists(ed::Settings::Instance().ConvertPath("temp/"), fsError))
		std::filesystem::create_directory(ed::Settings::Instance().ConvertPath("temp/"), fsError);

	// delete log.txt on startup
	if (std::filesystem::exists(ed::Settings::Instance().ConvertPath("log.txt"), fsError))
		std::filesystem::remove(ed::Settings::Instance().ConvertPath("log.txt"), fsError);

	// set stb_image flags
	stbi_flip_vertically_on_write(1);
	stbi_set_flip_vertically_on_load(1);

	// init sdl2
	if (!glfwInit()) {
		ed::Logger::Get().Log("Failed to initialize SDL2", true);
		ed::Logger::Get().Save();
		return 0;
	} else
		ed::Logger::Get().Log("Initialized SDL2");

	struct AppData {
		ed::EditorEngine* engine = nullptr;
		bool hasFocus = true;
		bool minimized = false;
		bool maximized = false;
		bool fullscreen = false;
		bool perfMode = false;
		short wndWidth = 800;
		short wndHeight = 600;
		short wndPosX = -1;
		short wndPosY = -1;
	} appData;

	bool& hasFocus = appData.hasFocus;
	bool& minimized = appData.minimized;
	bool& maximized = appData.maximized;
	bool& fullscreen = appData.fullscreen;
	bool& perfMode = appData.perfMode;
	short& wndWidth = appData.wndWidth;
	short& wndHeight = appData.wndHeight;
	short& wndPosX = appData.wndPosX;
	short& wndPosY = appData.wndPosY;

	// load window size
	std::string preloadDatPath = "data/preload.dat";
	if (!ed::Settings::Instance().LinuxHomeDirectory.empty() && std::filesystem::exists(ed::Settings::Instance().LinuxHomeDirectory + preloadDatPath, fsError))
		preloadDatPath = ed::Settings::Instance().ConvertPath(preloadDatPath);
	std::ifstream preload(preloadDatPath);
	if (preload.is_open()) {
		ed::Logger::Get().Log("Loading window information from data/preload.dat");

		preload.read(reinterpret_cast<char*>(&wndWidth), 2);
		preload.read(reinterpret_cast<char*>(&wndHeight), 2);
		preload.read(reinterpret_cast<char*>(&wndPosX), 2);
		preload.read(reinterpret_cast<char*>(&wndPosY), 2);
		fullscreen = preload.get();
		maximized = preload.get();
		if (preload.peek() != EOF)
			perfMode = preload.get();
		preload.close();

		// clamp to desktop size
		GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
		const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);
		if (mode && wndWidth > mode->width)
			wndWidth = mode->width;
		if (mode && wndHeight > mode->height)
			wndHeight = mode->height;
	} else {
		ed::Logger::Get().Log("File data/preload.dat doesnt exist", true);
		ed::Logger::Get().Log("Deleting data/workspace.dat", true);

		std::filesystem::remove("./data/workspace.dat", fsError);
	}

	// apply parsed CL options
	if (coptsParser.MinimalMode)
		maximized = false;
	if (coptsParser.WindowWidth > 0)
		wndWidth = coptsParser.WindowWidth;
	if (coptsParser.WindowHeight > 0)
		wndHeight = coptsParser.WindowHeight;
	perfMode = perfMode || coptsParser.PerformanceMode;
	fullscreen = fullscreen || coptsParser.Fullscreen;
	maximized = maximized || coptsParser.Maximized;

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);

	Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	// glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);


	bool run = true; // should we enter the infinite loop?
	// make the window invisible if only rendering to a file
	if (coptsParser.Render || coptsParser.ConvertCPP) {
		maximized = false;
		fullscreen = false;
		run = false;
	}

	// open window
	GLFWwindow* wnd = glfwCreateWindow(wndWidth, wndHeight, "SHADERed", NULL, NULL);
	if (!wnd) {
		glfwTerminate();
		return -1; // Handle error
	}
	glfwSetWindowPos(wnd, (wndPosX == -1) ? 100 : wndPosX, (wndPosY == -1) ? 100 : wndPosY);
	SetDpiAware();
	glfwSetWindowSizeLimits(wnd, 200, 200, GLFW_DONT_CARE, GLFW_DONT_CARE);

	if (maximized)
		glfwMaximizeWindow(wnd);
	if (fullscreen)
		; // not implemented in glfw

	// get GL context
	glfwMakeContextCurrent(wnd);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_STENCIL_TEST);

	// init glew
	glewExperimental = true;
	if (glewInit() != GLEW_OK) {
		ed::Logger::Get().Log("Failed to initialize GLEW", true);
		ed::Logger::Get().Save();
		return 0;
	} else
		ed::Logger::Get().Log("Initialized GLEW");

	// create engine
	appData.engine = new ed::EditorEngine(wnd);
	ed::EditorEngine& engine = *appData.engine;
	ed::Logger::Get().Log("Creating EditorEngine...");
	engine.Create();
	ed::Logger::Get().Log("Created EditorEngine");
	engine.Interface().Run = run;

	// set window icon:
	SetIcon(wnd);

	engine.UI().SetCommandLineOptions(coptsParser);
	engine.UI().SetPerformanceMode(perfMode);
	engine.Interface().Renderer.AllowComputeShaders(GLEW_ARB_compute_shader);
	engine.Interface().Renderer.AllowTessellationShaders(GLEW_ARB_tessellation_shader);

	// check for filesystem errors
	if (fsError)
		ed::Logger::Get().Log("A filesystem error has occured: " + fsError.message(), true);

	// loop through all OpenGL errors
	GLenum oglError;
	while ((oglError = glGetError()) != GL_NO_ERROR)
		ed::Logger::Get().Log("GL error: " + std::to_string(oglError), true);

	// convert .sprj -> cmake/c++
	if (coptsParser.ConvertCPP) {
		engine.UI().Open(coptsParser.ProjectFile);
		ed::ExportCPP::Export(&engine.Interface(), coptsParser.CMakePath, true, true, "ShaderProject", true, true, true);
		// return 0;
	}

	// render to file
	if (coptsParser.Render) {
		engine.UI().Open(coptsParser.ProjectFile);
		printf("Rendering to file...\n");
		engine.UI().SavePreviewToFile();
	}

	// start the DAP server
	if (coptsParser.StartDAPServer)
		engine.Interface().DAP.Initialize();

	// timer for time delta
	ed::eng::Timer timer;

	glfwSetWindowCloseCallback(wnd, [](GLFWwindow* window){
		auto d = reinterpret_cast<AppData*>(glfwGetWindowUserPointer(window));
		bool cont = true;
		if (d->engine->Interface().Parser.IsProjectModified()) {
			int btnID = d->engine->UI().AreYouSure();
			if (btnID == 2)
				cont = false;
		}

		if (cont) {
			d->engine->Interface().Run = false;
			ed::Logger::Get().Log("glfwWindowShouldClose -> quitting");
		}

		AppEvent e(AppEvent::WindowClose, window);
		d->engine->OnEvent(e);
	});
	glfwSetWindowFocusCallback(wnd, [](GLFWwindow *window, int focused){
		auto d = reinterpret_cast<AppData*>(glfwGetWindowUserPointer(window));
		d->hasFocus = focused;
		AppEvent e(AppEvent::WindowFocus, window);
		e.windowFocus.focused = focused;
		d->engine->OnEvent(e);
	});
	glfwSetWindowIconifyCallback(wnd, [](GLFWwindow* window, int iconified){
		auto d = reinterpret_cast<AppData*>(glfwGetWindowUserPointer(window));
		if (iconified) {
			d->minimized = true;
		} else {
			// should I restore by hand?
		}
		AppEvent e(AppEvent::WindowIconify, window);
		e.windowIconify.iconified = iconified;
		d->engine->OnEvent(e);
	});
	glfwSetWindowSizeCallback(wnd, [](GLFWwindow* window, int width, int height){
		auto d = reinterpret_cast<AppData*>(glfwGetWindowUserPointer(window));
		d->maximized = glfwGetWindowAttrib(window, GLFW_MAXIMIZED);
		d->fullscreen = false;
		d->minimized = glfwGetWindowAttrib(window, GLFW_ICONIFIED);

		// cache window size and position
		if (!d->maximized) {
			int tempX = 0, tempY = 0;
			glfwGetWindowPos(window, &tempX, &tempY);
			d->wndPosX = tempX;
			d->wndPosY = tempY;
			glfwGetWindowSize(window, &tempX, &tempY);
			d->wndWidth = tempX;
			d->wndHeight = tempY;
		}
		AppEvent e(AppEvent::WindowSize, window);
		e.windowSize.width = width;
		e.windowSize.height = height;
		d->engine->OnEvent(e);
	});
	glfwSetKeyCallback(wnd, [](GLFWwindow* window, int key, int scancode, int action, int mods){
		auto d = reinterpret_cast<AppData*>(glfwGetWindowUserPointer(window));
		AppEvent e(AppEvent::KeyPress, window);
		e.keyPress.key = key;
		e.keyPress.scancode = scancode;
		e.keyPress.action = action;
		e.keyPress.mods = mods;
		d->engine->OnEvent(e);
	});
	glfwSetCursorPosCallback(wnd, [](GLFWwindow* window, double xpos, double ypos){
		auto d = reinterpret_cast<AppData*>(glfwGetWindowUserPointer(window));
		AppEvent e(AppEvent::CursorPos, window);
		e.cursorPos.xpos = xpos;
		e.cursorPos.ypos = ypos;
		d->engine->OnEvent(e);
	});
	glfwSetMouseButtonCallback(wnd, [](GLFWwindow* window, int button, int action, int mods){
		auto d = reinterpret_cast<AppData*>(glfwGetWindowUserPointer(window));
		AppEvent e(AppEvent::MouseButton, window);
		e.mouseButton.button = button;
		e.mouseButton.action = action;
		e.mouseButton.mods = mods;
		d->engine->OnEvent(e);
	});
	glfwSetScrollCallback(wnd, [](GLFWwindow* window, double xoffset, double yoffset){
		auto d = reinterpret_cast<AppData*>(glfwGetWindowUserPointer(window));
		AppEvent e(AppEvent::Scroll, window);
		e.scroll.xoffset = xoffset;
		e.scroll.yoffset = yoffset;
		d->engine->OnEvent(e);
	});
	glfwSetCharCallback(wnd, [](GLFWwindow* window, unsigned int c){
		auto d = reinterpret_cast<AppData*>(glfwGetWindowUserPointer(window));
		AppEvent e(AppEvent::CharInput, window);
		e.charInput.c = c;
		d->engine->OnEvent(e);
	});

	while (engine.Interface().Run) {
		glfwPollEvents();
#if defined(_WIN32)
		// else if (event.type == SDL_SYSWMEVENT) {
		// 	// this doesn't work - it seems that SDL doesn't forward WM_DPICHANGED message
		// 	if (event.syswm.type == WM_DPICHANGED && ed::Settings::Instance().General.AutoScale) {
		// 		float dpi = 0.0f;
		// 		int wndDisplayIndex = SDL_GetWindowDisplayIndex(wnd);
		// 		SDL_GetDisplayDPI(wndDisplayIndex, &dpi, NULL, NULL);

		// 		if (dpi <= 0.0f) dpi = 1.0f;

		// 		ed::Settings::Instance().TempScale = dpi / 96.0f;
		// 		ed::Logger::Get().Log("Updating DPI to " + std::to_string(dpi / 96.0f));
		// 	}
		// }
#endif
		if (!engine.Interface().Run) break;

		float delta = timer.Restart();
		engine.Update(delta);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		engine.Render();
		glfwSwapBuffers(wnd);

		if (minimized && delta * 1000 < 33)
			std::this_thread::sleep_for(std::chrono::milliseconds(33 - (int)(delta * 1000)));
		else if (!hasFocus && ed::Settings::Instance().Preview.LostFocusLimitFPS && delta * 1000 < 16)
			std::this_thread::sleep_for(std::chrono::milliseconds(16 - (int)(delta * 1000)));
	}

	engine.UI().Destroy();

	// union for converting short to bytes
	union {
		short size;
		char data[2];
	} converter;

	// save window size
	preloadDatPath = ed::Settings::Instance().ConvertPath("data/preload.dat");
	if (!coptsParser.Render && !coptsParser.ConvertCPP) {
		ed::Logger::Get().Log("Saving window information");

		std::ofstream save(preloadDatPath);
		converter.size = wndWidth; // write window width
		save.write(converter.data, 2);
		converter.size = wndHeight; // write window height
		save.write(converter.data, 2);
		converter.size = wndPosX; // write window position x
		save.write(converter.data, 2);
		converter.size = wndPosY; // write window position y
		save.write(converter.data, 2);
		save.put(fullscreen);
		save.put(maximized);
		save.put(engine.UI().IsPerformanceMode());
		save.write(converter.data, 2);
		save.close();
	}

	// close and free the memory
	engine.Destroy();

	// glfw
	glfwDestroyWindow(wnd);
	glfwTerminate();

	ed::Logger::Get().Log("Destroyed EditorEngine and SDL2");

	ed::Logger::Get().Save();

	return 0;
}

void SetIcon(GLFWwindow* wnd)
{
	// GLFWimage images[2];
	// images[0] = load_icon("my_icon.png");
	// images[1] = load_icon("my_icon_small.png");
	
	// glfwSetWindowIcon(window, 2, images);

	// float dpi = 0.0f;
	// int wndDisplayIndex = SDL_GetWindowDisplayIndex(wnd);
	// SDL_GetDisplayDPI(wndDisplayIndex, &dpi, NULL, NULL);
	// dpi /= 96.0f;

	// if (dpi <= 0.0f) dpi = 1.0f;

	// stbi_set_flip_vertically_on_load(0);

	// int req_format = STBI_rgb_alpha;
	// int width, height, orig_format;
	// unsigned char* data = stbi_load(dpi == 1.0f ? "./icon_64x64.png" : "./icon_256x256.png", &width, &height, &orig_format, req_format);
	// if (data == NULL) {
	// 	ed::Logger::Get().Log("Failed to set window icon", true);
	// 	return;
	// }

	// int depth, pitch;
	// Uint32 pixel_format;
	// if (req_format == STBI_rgb) {
	// 	depth = 24;
	// 	pitch = 3 * width; // 3 bytes per pixel * pixels per row
	// 	pixel_format = SDL_PIXELFORMAT_RGB24;
	// } else { // STBI_rgb_alpha (RGBA)
	// 	depth = 32;
	// 	pitch = 4 * width;
	// 	pixel_format = SDL_PIXELFORMAT_RGBA32;
	// }

	// SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormatFrom((void*)data, width, height,
	// 	depth, pitch, pixel_format);

	// if (surf == NULL) {
	// 	ed::Logger::Get().Log("Failed to create icon SDL_Surface", true);
	// 	stbi_image_free(data);
	// 	return;
	// }

	// SDL_SetWindowIcon(wnd, surf);

	// SDL_FreeSurface(surf);
	// stbi_image_free(data);

	// stbi_set_flip_vertically_on_load(1);
}
void SetDpiAware()
{
#if defined(_WIN32)
	enum DpiAwareness {
		Unaware,
		System,
		PerMonitor
	};
	typedef HRESULT(WINAPI * SetProcessDpiAwarenessFn)(DpiAwareness);
	typedef BOOL(WINAPI * SetProcessDPIAwareFn)(void);

	HINSTANCE lib = LoadLibraryA("Shcore.dll");

	if (lib) {
		SetProcessDpiAwarenessFn setProcessDpiAwareness = (SetProcessDpiAwarenessFn)GetProcAddress(lib, "SetProcessDpiAwareness");
		if (setProcessDpiAwareness)
			setProcessDpiAwareness(DpiAwareness::PerMonitor);
	} else {
		lib = LoadLibraryA("user32.dll");
		if (lib) {
			SetProcessDPIAwareFn setProcessDPIAware = (SetProcessDPIAwareFn)GetProcAddress(lib, "SetProcessDPIAware");

			if (setProcessDPIAware)
				setProcessDPIAware();
		}
	}

	if (lib)
		FreeLibrary(lib);
#endif
}
