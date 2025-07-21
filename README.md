Тестовий консольний проєкт на C++ (функція main()) для Visual Studio 2019.

Для доступу до OpenAI використовуються curl, в программі передбачена можливість використовувати Python замість curl для доступу до OpenAI.

Проєкт компілювати як Debug x86 (оскільки збірка curl Debug x86).

Залежності для компіляції программи (біблиотеки):

1) бібліотека curl (наявна в папці з проєктом) для надсилання/отримання запитів до OpenAI, дана програма використовує 32-бітну debug версію бібліотеки curl, тому проєкт потрібно компілювати як Debug x86

2) Python версії 3 для Windows 10 завантажити та встановити за посиланням:

https://www.python.org/downloads/

дуже важливо — на першому екрані встановлення поставити галочку Add Python 3.x to PATH

перевірити, що Python встановлено — запустити cmd.exe та ввести команду:
python --version

там же в cmd.exe ввести команду встановлення модуля OpenAI для Python:
python -m pip install openai

3) бібліотека для роботи з JSON nlohmann\json.hpp (наявна в папці з проєктом),
бібліотека nlohmann завантажена за наступною адресою:
https://github.com/nlohmann/json/blob/develop/single_include/nlohmann/json.hpp

Залежності для компіляції програми — необхідно змінити значення трьох змінних.

На початку файлу cpp із програмою є такий, як нижче, блок коду з коментарями для заміни значень трьох змінних:

//змінна #1 змінити для правильної роботи програми
//шлях до vcvars32.bat для налаштування компілятора cl.exe

static std::string vcvarsPath = "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Enterprise\\VC\\Auxiliary\\Build\\vcvars32.bat";

//змінна #2 змінити для правильної роботи програми
//шлях до папки, в якій розміщена папка з JSON бібліотекою nlohmann\json.hpp

static std::string includePath = "C:\\Users\\black4\\Desktop\\test-kurlyak-debug-x86\\Sample";

//змінна #3 змінити для правильної роботи програми
//API ключ

static const char* ApiKeyStr = "";

Після клонування проекту GitHub у Visual Stduio 2019 (як вже говорилося) необхідно виставити проект як Debug x86, та у властивостях проекту вказати С++ 17.
