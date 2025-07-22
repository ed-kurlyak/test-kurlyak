#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <filesystem>

#include ".\nlohmann\json.hpp"
#include ".\curl_builds\libcurl-vc-x86\include\curl\curl.h"

#pragma comment(lib, ".\\curl_builds\\libcurl-vc-x86\\lib\\libcurl_debug.lib")

//за замовчуванням для доступу до OpenAI використовується CURL
//поставте 0 якщо вам потрібно доступ за допомогою Python
#define USE_CURL 1

//кількість упішних ітерацій яка потрібна
#define ITERATIONS_NEED_COUNT 15

namespace fs = std::filesystem;

#define PI 3.1415926535897932384626433
#define EARTH_RADIUS 6371000.0 // радiус Землi в метрах

/***********************************************************************
обов'язково поміняти три змінні нижче для правильної роботи программи
************************************************************************/

//змінна #1 змінити для правильної роботи программи
//шлях до vcvars32.bat для настройки компілятора cl.exe
static std::string g_VcvarsPath = "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Enterprise\\VC\\Auxiliary\\Build\\vcvars32.bat";

//змінна #2 змінити для правильної роботи программи
//шлях до папки в якій розміщена папка з JSON бібліотекою nlohmann\json.hpp
static std::string g_IncludePath = "C:\\Users\\black4\\Desktop\\test-kurlyak-debug-x86\\Sample";

//змінна #3 змінити для правильної роботи программи
//API ключ
static const char* g_ApiKeyStr = "";

using json = nlohmann::json;

//в цей файл записуємо всі результати result.txt
std::ofstream g_File;

//структура для зберігання точки
struct Point
{
	long long lat;
	long long lon;
	long long time;
};

//три змінні зберігають час обробки для кожного json файлу
static long long g_Dt;
static long long g_Dt2;
static long long g_Dt3;

//головний запит до ChatGPT
static const std::string g_MainPromptToChatGPT = u8R"(
Згенеруй програму на C++, яка:

ЗАГАЛЬНІ ВИМОГИ:

1. Зчитує JSON-файл points.json зі стандартного вводу (stdin), що містить масив об’єктів з такими полями:
{
  "lat": long long, // GPS широта в форматі E5 (lat * 1e5)
  "lon": long long, // GPS довгота в форматі E5 (lon * 1e5)
  "time": long long // Unix час (в секундах)
}

2. Обробляє дані: визначає аномальні точки, де спостерігається стрибок більш ніж 1000 метрів за 5 секунд або менше.

3. Виправляє ці точки методом лінійної інтерполяції на основі:
- медіанної швидкості руху
- напрямку руху у відрізку навколо аномальної точки

4. Виводить скорегований JSON у стандартний вивід (stdout) у тому ж форматі, але вже з виправленими lat і lon (поле time не змінюється).

5. Програма повинна запускатися з командного рядка наступним чином:
algorithm.exe < points.json > corrected.json

АЛГОРИТМ РОБОТИ ПРОГРАМИ:

1. Завантажити всі точки в std::vector<Point>:
struct Point
{
    long long lat;
    long long lon;
    long long time;
};

2. Перетворити lat і lon з типу long long в double:
double lat = point.lat / 100000.0;
double lon = point.lon / 100000.0;

3. За допомогою алгоритму Хаверсина обчислити відстані між усіма сусідніми точками і відповідну швидкість.

4. Знайти медіанну швидкість руху на основі обчислених значень.

5. Виявити аномальні точки:
- Якщо між двома точками швидкість перевищує 1000 м за 5 секунд – точка вважається аномальною.
- Для кожної такої точки:
  - обчислити очікуваний напрямок руху між попередньою та наступною точками
  - за допомогою медіанної швидкості та часу – інтерполювати координати
  - записати нові координати точки замість аномальних

6. Отримані координати double lat, double lon після обробки перетворити назад у формат E5:
point.lat = static_cast<long long>(lat * 100000);
point.lon = static_cast<long long>(lon * 100000);

7. Записати виправлений масив точок у форматі JSON в stdout.

ТЕХНІЧНІ ВИМОГИ:

- Програма має бути сумісна з Visual Studio 2019.
- Використовуй бібліотеку nlohmann/json версії 3.12.0:
- Біблиотека nlohmann знаходиться в поточній папці,
бібліотеку nlohmann в cpp файл підключай наступним чином:
#include "./nlohmann/json.hpp"
(не використовуй #include <nlohmann/json.hpp>)

- Не використовуй M_PI, натомість додай:
#define PI 3.1415926535897932384626433

- Не виводь жодних логів у вихідний JSON (не додавай "Anomaly detected" чи подібне).

Це має бути єдиний самодостатній .cpp файл, який компілюється без помилок і працює відповідно до вказаних вимог.
)";

//поточний запит до ChatGPT зберігається тут
static std::string g_CurrentPromptToChatGPT;

//вектор зберігає середній час виконання кожної ітерації
//якщо 15 ітерацій то вектор має 15 елементів
static std::vector<long long> g_TimeBuff;

//лічильник ітерацій - вдало скомпілювалося
static auto g_IterationsOkCount = 0;
//лічильник ітерацій - всіх вдалих і невдалих
static auto g_IterationsAllCount = 0;

//прототипи функцій
static void Reset_File();
static void Close_File();
static size_t Write_Callback(void* contents, size_t size, size_t nmemb, void* userp);
static bool Send_To_OpenAI(const std::string& g_CurrentPromptToChatGPT, std::string& response);
static int Execution(std::string FileIn, std::string FileOut, long long& g_Dt);
static double Haversine(double lat1, double lon1, double lat2, double lon2);
static void Check_Anomalies_And_Speed(const std::string& FileIn);
static int Send_Request_To_OpenAI_And_Get_CPP_File_CURL();
static int Try_Compile_Generated_Cpp_File();
static int Try_Execute_Compiled_File();
static int Test_Animalies_In_JSON();
static int Create_Traget_Directory_And_Copy_Result();
static bool CheckFile(const std::string& Path);
static void Delete_Files();
static int Send_Request_To_OpenAI_And_Get_CPP_File_Python();
static void Write_Current_Prompt_To_File(const std::string& filename = "temp_prompt.txt");
static std::string Extract_Cpp_Code(const std::string& Text);
static bool Run_Python_ChatGpt_From_File(const std::string& PromptFile, std::string& Resp);
static void Create_Directory(std::string FolderName);
static void Copy_Files_To_Directory(std::string FolderName);

static bool CheckFile(const std::string& Path)
{
	if (!fs::exists(Path))
	{
		std::cout << "File " << Path << " is not exists\n";
		return false;
	}

	if (fs::file_size(Path) == 0)
	{
		std::cout << "File " << Path << " is empty\n";
		return false;
	}

	return true;
}

//функція створює файл результату result.txt
static void Reset_File()
{
	g_File.open("result.txt", std::ios::out | std::ios::trunc);

	if (!g_File.is_open())
	{
		std::cerr << "Error: cannot open file result.txt for writing\n";
	}
	else
	{
		//std::cout << "файл result.txt очищений\n";
	}
}

static void Close_File()
{
	if (g_File.is_open())
	{
		g_File.close();
	}
}

//збереження відповіді від OpenAI
static size_t Write_Callback(void* Contents, size_t Size, size_t Nmemb, void* Userp)
{
	((std::string*)Userp)->append((char*)Contents, Size * Nmemb);
	
	return Size * Nmemb;
}

static bool Send_To_OpenAI(const std::string& g_CurrentPromptToChatGPT, std::string& Response)
{
	CURL* Curl = curl_easy_init();
	if (!Curl)
		return false;

	curl_easy_setopt(Curl, CURLOPT_URL, "https://api.openai.com/v1/chat/completions");

	struct curl_slist* Headers = nullptr;
	Headers = curl_slist_append(Headers, "Content-Type: application/json");

	std::string AuthHeader = "Authorization: Bearer " + std::string(g_ApiKeyStr);
	Headers = curl_slist_append(Headers, AuthHeader.c_str());

	curl_easy_setopt(Curl, CURLOPT_HTTPHEADER, Headers);

	nlohmann::json Body = {
		{"model", "gpt-4o-mini"},
		{"messages", {{{"role", "user"}, {"content", g_CurrentPromptToChatGPT}}}}
	};

	std::string bodys = Body.dump();
	curl_easy_setopt(Curl, CURLOPT_POSTFIELDS, bodys.c_str());

	curl_easy_setopt(Curl, CURLOPT_WRITEFUNCTION, Write_Callback);
	curl_easy_setopt(Curl, CURLOPT_WRITEDATA, &Response);

	CURLcode Res = curl_easy_perform(Curl);

	curl_slist_free_all(Headers);
	curl_easy_cleanup(Curl);

	return (Res == CURLE_OK);
}

[[nodiscard]]
static int Execution(std::string FileIn, std::string FileOut, long long& g_Dt)
{
	//виконання та вимір часу
	auto t0 = std::chrono::high_resolution_clock::now();

	std::string CmdLine = "algorithm.exe < " + FileIn + " > " + FileOut + " 2> execution_log.txt";

	int ErrorResult = std::system(CmdLine.c_str());

	auto t1 = std::chrono::high_resolution_clock::now();

	g_Dt = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

	if (ErrorResult != 0)
	{
		return 0;
	}

	std::cout << "Execution OK " << FileIn << " time " << g_Dt << " ms " << "Result stored in: " << FileOut << "\n";

	if (!g_File.is_open())
	{
		std::cerr << "Error: File is not open\n";
	}
	else
	{
		g_File << "Execution OK " << FileIn << " time " << g_Dt << " ms " << "Result stored in: " << FileOut << "\n";
	}

	return 1;
}

//функція для обчислення відстані між двома точками (формула Гаверсинуса)
static double Haversine(double Lat1, double Lon1, double Lat2, double Lon2)
{
	double DeltaLat = (Lat2 - Lat1) * PI / 180.0;
	double DeltaLon = (Lon2 - Lon1) * PI / 180.0;

	Lat1 = Lat1 * PI / 180.0;
	Lat2 = Lat2 * PI / 180.0;

	double a = pow(sin(DeltaLat / 2), 2) + pow(sin(DeltaLon / 2), 2) * cos(Lat1) * cos(Lat2);
	double c = 2 * atan2(sqrt(a), sqrt(1 - a));

	return EARTH_RADIUS * c;
}

//основна функція для перевірки аномалій
static void Check_Anomalies_And_Speed(const std::string& FileIn)
{
	if (!CheckFile(FileIn))
		return;

	std::ifstream In(FileIn);

	if (!In.is_open())
	{
		std::cerr << "Failed to open File: " << FileIn << std::endl;
		return;
	}

	json Data;

	try
	{
		In >> Data;
	}
	catch (const std::exception& e)
	{
		std::cerr << "Error while reading JSON: " << e.what() << std::endl;
		return;
	}

	std::vector<Point> Points;

	for (const auto& Item : Data)
	{
		if (!Item.contains("lat") || !Item.contains("lon") || !Item.contains("time"))
			continue;

		Point Pt;

		Pt.lat = Item["lat"];
		Pt.lon = Item["lon"];
		Pt.time = Item["time"];

		Points.push_back(Pt);
	}

	if (Points.size() < 2)
	{
		std::cout << "Not enough points to analyze" << std::endl;
		return;
	}

	bool HasAnomalies = false;

	for (size_t i = 1; i < Points.size(); ++i)
	{
		double distance = Haversine(Points[i - 1].lat / 1000000.0, Points[i - 1].lon / 1000000.0,
			Points[i].lat / 1000000.0, Points[i].lon / 1000000.0);

		long long time_diff = std::abs(Points[i].time - Points[i - 1].time);

		//розрахунок швидкості
		if (time_diff == 0)
		{
			/*
			std::cout << "speed between points " << i - 1 << " and " << i << ": undefined (zero time)" << std::endl;
			*/
		}
		else
		{
			double speed = distance / static_cast<double>(time_diff); // m/s
			/*
			std::cout << "speed between points " << i - 1 << " and " << i << ": " << speed << " m/s" << std::endl;
			*/
		}

		//перевірка на аномалії
		if (distance > 1000.0 && time_diff <= 5)
		{
			/*
			std::cout << "anomaly between points " << i - 1 << " and " << i << ": "	<< "distance = " << distance << " m, time = " << time_diff << " s" << std::endl;
			*/
			HasAnomalies = true;
		}
	}

	if (!g_File.is_open())
	{
		std::cerr << "Error: File is not open\n";
		return;
	}

	if (!HasAnomalies)
	{
		std::cout << FileIn << " No anomalous points found" << std::endl;

		g_File << FileIn << " No anomalous points found" << std::endl;
	}
	else
	{
		std::cout << FileIn << " There are anomalous points found" << std::endl;
		
		g_File << FileIn << " There are anomalous points found" << std::endl;
	}
}

static void Write_Current_Prompt_To_File(const std::string& Filename)
{
	std::ofstream File(Filename);
	if (File.is_open())
	{
		File << g_CurrentPromptToChatGPT;
		File.close();
	}
}

[[nodiscard]]
static std::string Extract_Cpp_Code(const std::string& Text)
{
	const std::string startTag = "```cpp";
	const std::string endTag = "```";

	size_t startPos = Text.find(startTag);
	if (startPos == std::string::npos)
	{
		return ""; //не знайдено початок блоку
	}

	startPos += startTag.length();

	size_t endPos = Text.find(endTag, startPos);
	if (endPos == std::string::npos)
	{
		return ""; //не знайдено кінець блоку
	}


	//вирізаємо код між тегами з урахуванням можливих переносів рядків і пробілів
	//можна видалити перший символ переносу рядка, якщо він є

	size_t codeStart = startPos;
	if (Text[codeStart] == '\n' || Text[codeStart] == '\r')
	{
		++codeStart;
		if (Text[codeStart] == '\n') //на випадок Windows \r\n
			++codeStart;
	}

	return Text.substr(codeStart, endPos - codeStart);
}

[[nodiscard]]
static bool Run_Python_ChatGpt_From_File(const std::string& PromptFile, std::string& Resp)
{
	std::string Command = "python chatgpt_request.py " + PromptFile + " " + g_ApiKeyStr;

	std::string Result;
	char Buffer[128];

	FILE* Pipe = _popen(Command.c_str(), "r");

	if (!Pipe)
	{
		return 0;
	}

	while (fgets(Buffer, sizeof(Buffer), Pipe) != nullptr)
	{
		Resp += Buffer;
	}

	_pclose(Pipe);

	return 1;
}

[[nodiscard]]
static int Send_Request_To_OpenAI_And_Get_CPP_File_Python()
{

	if (!CheckFile("chatgpt_request.py"))
	{
		std::cout << "There is no chatgpt_request.py file" << std::endl;
		exit(0);
	}

	std::string Resp;

	//записуємо prompt у файл, бо Python-скрипт не коректно сприймає багаторядкові рядки з переносами \n
	//тож передаємо текст через файл, щоб уникнути проблем з форматуванням рядка у виклику Python
	Write_Current_Prompt_To_File();

	if (!Run_Python_ChatGpt_From_File("temp_prompt.txt", Resp))
	{
		std::cerr << "GPT request failed - exit program - check your internet connection\n";
		exit(0);
	}

	std::string Code = Extract_Cpp_Code(Resp);

	if (Code != "")
	{
		std::ofstream Ofs("generated_code.cpp");
		Ofs << Code;
		Ofs.close();

	}
	else
	{
		std::cerr << "No code block found\n";

		g_CurrentPromptToChatGPT = g_MainPromptToChatGPT;

		return 0;
	}


	return 1;
}


[[nodiscard]]
static int Send_Request_To_OpenAI_And_Get_CPP_File_CURL()
{
	std::string Resp;

	if (!Send_To_OpenAI(g_CurrentPromptToChatGPT, Resp))
	{
		std::cerr << "GPT request failed - exit program - check your internet connection\n";
		exit(0);
	}

	nlohmann::json j = nlohmann::json::parse(Resp);

	if (j.contains("error") && j["error"].contains("message"))
	{
		std::string errorMsg = j["error"]["message"];
		MessageBoxA(NULL, errorMsg.c_str(), "Error", MB_OK | MB_ICONERROR);

		exit(0);
	}

	Resp = j["choices"][0]["message"]["content"];

	//зберігаємо лише код між ```cpp ... ```
	auto start = Resp.find("```cpp");
	auto end = Resp.find("```", start + 1);

	std::string Code;

	if (start != std::string::npos && end != std::string::npos && end > start)
	{
		Code = Resp.substr(start + 7, end - (start + 7));
	}
	else
	{
		std::cerr << "No code block found\n";

		g_CurrentPromptToChatGPT = g_MainPromptToChatGPT;

		return 0;
	}

	std::ofstream Ofs("generated_code.cpp");
	Ofs << Code;
	Ofs.close();

	return 1;
}

[[nodiscard]]
static int Try_Compile_Generated_Cpp_File()
{

	std::string Cmd =
		"cmd /C \""
		"\"" + g_VcvarsPath + "\" && "
		"cl.exe /EHsc generated_code.cpp /I \"" + g_IncludePath + "\" /Fe:algorithm.exe"
		"\" > compile_log.txt";

	int Cres = std::system(Cmd.c_str());

	if (Cres != 0)
	{
		std::cout << "Compilation failed" << std::endl << std::endl;

		//читаємо журнал компіляції
		std::ifstream LogFile("compile_log.txt");

		if (LogFile)
		{
			std::string LogContent((std::istreambuf_iterator<char>(LogFile)),
				std::istreambuf_iterator<char>());

			LogFile.close();

			g_CurrentPromptToChatGPT = g_MainPromptToChatGPT;

			//додаємо лог і вказівку згенерувати весь код заново
			g_CurrentPromptToChatGPT += u8"\nCompiler log:\n" + LogContent + "\n";
			g_CurrentPromptToChatGPT += u8"Виправ помилки. Сгенеруй весь код заново.\n";

			std::cout << "\n";

			return 0;
		}
		else
		{
			g_CurrentPromptToChatGPT = g_MainPromptToChatGPT;

			g_CurrentPromptToChatGPT += u8"\nCompilation failed, log not available.\n";
			g_CurrentPromptToChatGPT += u8"Виправ помилки. Сгенеруй весь код заново.\n";

			std::cout << "\n";

			return 0;
		}
	}

	return 1;

}

[[nodiscard]]
static int Try_Execute_Compiled_File()
{
	if (!CheckFile("algorithm.exe"))
	{
		std::cerr << "Execution failed\n";

		std::cout << "\n";

		g_CurrentPromptToChatGPT = g_MainPromptToChatGPT;
		//додаємо лог і вказівку згенерувати весь код заново
		g_CurrentPromptToChatGPT += u8"Ти згенерував С++ код але компіляція завершилася з помилкою - EXE файлу не існує, виправ помилку.\n";

		std::cout << "\n";

		return 0;
	}

	Close_File();
	Reset_File();

	g_File << "Execution test result:" << std::endl;

	if (!Execution("points.json", "corrected.json", g_Dt) ||
		!Execution("points2.json", "corrected2.json", g_Dt2) ||
		!Execution("points3.json", "corrected3.json", g_Dt3))
	{
		std::cerr << "Execution failed\n";

		std::cout << "\n";

		std::ifstream LogFile("execution_log.txt");

		if (LogFile)
		{
			std::string LogContent((std::istreambuf_iterator<char>(LogFile)),
				std::istreambuf_iterator<char>());

			LogFile.close();

			//додаємо лог і вказівку згенерувати весь код заново
			g_CurrentPromptToChatGPT = u8"Ти згенерував С++ код але після компіляції і запуску він видає помилку виконання:\n" + LogContent + u8", виправ помилку.\n";

		}

		g_CurrentPromptToChatGPT += u8"Помилка виконання runtime error. Згенеруй код файла cpp повторно. ";
		g_CurrentPromptToChatGPT += g_MainPromptToChatGPT;

		std::cout << "\n";

		return 0;
	}

	std::cout << std::endl;

	return 1;

}

static int Test_Animalies_In_JSON()
{

	g_File << "\n\n";
	g_File << "Test anomalies result" << std::endl;

	Check_Anomalies_And_Speed("corrected.json");
	Check_Anomalies_And_Speed("corrected2.json");
	Check_Anomalies_And_Speed("corrected3.json");

	Close_File();

	return 1;
}

static void Create_Directory(std::string FolderName)
{
	std::error_code ErrorCode;

	if (fs::create_directory(FolderName, ErrorCode))
	{
		//std::cout << "Folder " << folderName << " created\n";
	}
	else
	{
		if (ErrorCode)
		{
			std::cerr << "Created folder error " << FolderName << ": " << ErrorCode.message() << "\n";
		}
		else
		{
			std::cout << "The folder " << FolderName << " already exists\n";
		}
	}
}

static void Copy_Files_To_Directory(std::string FolderName)
{
	//файли результату ітерації копіюються в окремі папки для кожної ітерації
	static std::string FilesToCopy[] = {
	"algorithm.exe",
	"points.json",
	"points2.json",
	"points3.json",
	"corrected.json",
	"corrected2.json",
	"corrected3.json",
	"generated_code.cpp",
	"result.txt",
	"compile_log.txt" };

	std::error_code ErrorCode;

	//копіюємо файли в папку
	for (const auto& File : FilesToCopy)
	{
		fs::path Source = File;
		fs::path Destination = fs::path(FolderName) / File;

		// copy_options::overwrite_existing перезаписуємо файл якщо він є
		fs::copy_file(Source, Destination, fs::copy_options::overwrite_existing, ErrorCode);

		if (ErrorCode)
		{
			std::cerr << "Error copying File " << File << " to " << FolderName << ": " << ErrorCode.message() << "\n";
		}
		else
		{
			//std::cout << "File " << File << " copied to " << folderName << "\n";
		}
	}

}

static int Create_Traget_Directory_And_Copy_Result()
{

	long long dTimeMain = (g_Dt + g_Dt2 + g_Dt3) / 3;

	//заносимо в массив ітерацій час данної ітерації
	g_TimeBuff.push_back(dTimeMain);

	std::string Duration = std::to_string(dTimeMain) + "ms";

	//створюємо папку для файлів ітерації

	char Buffer[256];
	sprintf_s(Buffer, 256, "%03d", g_IterationsOkCount + 1);

	std::string FolderName = "Interation" + std::string(Buffer) + "-" + Duration;

	Create_Directory(FolderName);
	
	Copy_Files_To_Directory(FolderName);
	
	return 1;
}

static void Delete_Files()
{
	//список тимчасових файлів які треба видалити після роботи программи
	static std::string FilesToDelete[] = {
	"algorithm.exe",
	"corrected.json",
	"corrected2.json",
	"corrected3.json",
	"generated_code.cpp",
	"generated_code.obj",
	"result.txt",
	"execution_log.txt",
	"compile_log.txt",
	"temp_prompt.txt" };

	//видаляємо тимчасові файли
	for (const std::string& File : FilesToDelete)
	{
		std::error_code ErrorCode;
		if (fs::remove(File, ErrorCode))
		{
			//std::cout << "Deleted: " << File << std::endl;
		}
		else
		{
			//std::cout << "Could not delete: " << File << " (" << ec.message() << ")" << std::endl;
		}
	}

}

int main()
{
	if (strlen(g_ApiKeyStr) == 0)
	{
		std::cout << "Please set API Key and run program again" << std::endl;
		return 0;
	}

	//текст запиту до ChatGPT
	g_CurrentPromptToChatGPT = g_MainPromptToChatGPT;

	while(g_IterationsOkCount < ITERATIONS_NEED_COUNT)
	{
		//видаляємо тимчасові файли від попередньої ітерації (якщо файли є)
		Delete_Files();

		//починаємо з одиниці
		//збільшуємо значення усіх ітерацій
		g_IterationsAllCount++;

		std::cout << "=======================================================================" << std::endl;
		std::cout << "=== Current Iteration # " << g_IterationsAllCount << " === " << "Iteration Succeeded " << g_IterationsOkCount << "/" << ITERATIONS_NEED_COUNT << " ================\n\n";

		//крок 1 - посилаємо запит до OpenAI та отримуємо CPP файл

		std::cout << "Step 1/4 - Sending request to ChatGPT to get cpp File - please wait..." << std::endl << std::endl;

#ifdef USE_CURL
		
		if (!Send_Request_To_OpenAI_And_Get_CPP_File_CURL())
		{
			continue;
		}
#else		
		if (!Send_Request_To_OpenAI_And_Get_CPP_File_Python())
		{
			continue;
		}
#endif

		//крок 2 - намагаємося скомпілювати CPP файл

		std::cout << "Step 2/4 - Trying to compile generated cpp File" << std::endl << std::endl;

		if (!Try_Compile_Generated_Cpp_File())
		{
			continue;
		}

		//крок 3 - виконуємо скомпільований CPP файл для трьох різних json файлів

		std::cout << "Compilation OK - Start execution" << std::endl << std::endl;
		std::cout << "Step 3/4 - Execution starts" << std::endl << std::endl;

		if (!Try_Execute_Compiled_File())
		{
			continue;
		}

		//крок 4 - перевіряємо на аномалії отимані скореговані json файли

		std::cout << "Step 4/4 - Test anomalies" << std::endl << std::endl;

		Test_Animalies_In_JSON();

		//крок 5 - копіємо всі результати в окрему папку для цієї успішної ітерації
		Create_Traget_Directory_And_Copy_Result();

		//генерація нового g_CurrentPromptToChatGPT з фідбеком:
		g_CurrentPromptToChatGPT = g_MainPromptToChatGPT;
		g_CurrentPromptToChatGPT += u8" Previous run took " + std::to_string(g_Dt) + u8" ms. " +
			u8"Зменши час виконнання программи і переконайся, що аномалії виправлені.";

		std::cout << "\n";

		//збільшуємо значення успішних ітерацій
		g_IterationsOkCount++;
	}

	Delete_Files();

	//в массиві часу всіх ітерацій вибираємо наймеший час і індекс в массиві
	auto It = std::min_element(g_TimeBuff.begin(), g_TimeBuff.end());

	if (It != g_TimeBuff.end())
	{
		//мінімальне значення з массиву
		long long MinValue = *It;

		//індекс цього мінімального значення в массиві
		int Index = static_cast<int>(std::distance(g_TimeBuff.begin(), It));

		//виводимо результат работи программи на екран

		char Buffer[256];
		sprintf_s(Buffer, 256, "%03d", Index + 1);

		std::string BestValue = "Interation" + std::string(Buffer) + "-" + std::to_string(MinValue);

		std::cout << std::endl;

		std::cout << "================================" << std::endl;

		std::cout << "Best Value is " << BestValue << std::endl;
		std::cout << "Count of All iterations " << g_IterationsAllCount << std::endl;
		std::cout << "Count of Succeeded iterations " << g_IterationsOkCount << std::endl;

		std::cout << "================================" << std::endl;
	}
	else
	{
		std::cout << "Немає найменшого часу викоання" << std::endl;
	}

	return 0;
}
