// Downloads current weather conditions from Berlin (per http get request) and saves it to SQLite DB

// Setup Windows 10:
// VS code: Each library needs to be downloaded (Precompiled Binaries and Header files) and linked to compiler
// Visual Studio: Use "C++ for desktop development" and install package manager vcpkg. Example: C:\dev\vcpkg>.\vcpkg install sqlite3:x64-windows
// Additionally to curl the code for wininet (based on windows kits sdk) is also provided


#include <iostream>
#include <fstream>
#include <ctime>
#include "nlohmann/json.hpp"
#include "sqlite3.h"
#include <curl/curl.h>
#include <chrono>
#include <thread>
using json = nlohmann::json;


size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t totalSize = size * nmemb;
    output->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}


std::string getWeatherData(std::string cityName) {
    std::string apiKey = "*******************************";
    std::string url = "http://api.weatherapi.com/v1/current.json?key=" + apiKey + "&q=" + cityName;
    CURL* curl = curl_easy_init();
    if (curl) {
        std::string weatherData;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &weatherData);
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "Failed to perform curl request: " << curl_easy_strerror(res) << std::endl;
            curl_easy_cleanup(curl);
            return "";
        }
        curl_easy_cleanup(curl);
        return weatherData;
    }
    std::cerr << "Failed to initialize curl" << std::endl;
    return "";
}


/*
std::string getWeatherData_WININET() {
    std::string apiKey = "*******************************";
    std::string url = "http://api.weatherapi.com/v1/current.json?key=" + apiKey + "&q=Berlin";

    HINTERNET hInternet = InternetOpenA("WeatherAPI", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (hInternet == NULL) {
        std::cerr << "Failed to initialize WinINet" << std::endl;
        return "";
    }

    HINTERNET hConnect = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (hConnect == NULL) {
        std::cerr << "Failed to open URL" << std::endl;
        InternetCloseHandle(hInternet);
        return "";
    }

    std::string weatherData;
    char buffer[4096];
    DWORD bytesRead;
    while (InternetReadFile(hConnect, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        weatherData.append(buffer, bytesRead);
    }

    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    return weatherData;
}
*/


std::string getCurrentDateTimeString() {
    std::time_t now = std::time(0);
    // std::tm* timeInfo = std::localtime(&now);
    std::tm timeInfo;
    localtime_s(&timeInfo, &now);
    char timeBuffer[20];
    std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &timeInfo);
    return timeBuffer;
}


void extractValues(std::string& dataW, std::string& coordinatesD, std::string& conditionD, std::string& timeD) {
    json parsedJson = json::parse(dataW);
    float lat = parsedJson["location"]["lat"];
    float lon = parsedJson["location"]["lon"];
    coordinatesD = std::to_string(lat) + " - " + std::to_string(lon);
    conditionD = parsedJson["current"]["condition"]["text"];
    timeD = getCurrentDateTimeString();
}


void saveWeatherToFile(const std::string& fileName, std::string& coordinatesD, std::string& conditionD, std::string& timeD) {
    std::ofstream file(fileName);
    if (file.is_open()) {
        file << "Current Time: " << timeD << "\n";
        file << "Coordinates: " << coordinatesD << "\n";
        file << "Weather Condition: " << conditionD << "\n";
        file.close();
        std::cout << "Weather data saved (.txt): " << timeD << " -> " << conditionD << "\n";
    }
    else {
        std::cerr << "Unable to open file: " << fileName << std::endl;
    }
}


bool initializeTable(sqlite3* db, std::string tableName) {
    std::string createTableQuery = "CREATE TABLE IF NOT EXISTS " + tableName + 
        " (time TEXT, coordinates TEXT, condition TEXT); ";
    char* errorMessage;
    int result = sqlite3_exec(db, createTableQuery.c_str(), nullptr, nullptr, &errorMessage);
    if (result != SQLITE_OK) {
        std::cerr << "Error creating table: " << errorMessage << std::endl;
        sqlite3_free(errorMessage);
        return false;
    }
    return true;
}


void insertValuesIntoTable(sqlite3* db, std::string tableName, std::unordered_map<std::string, std::string> dataMap) {
    std::string columnNames = "";
    std::string columnValues = "'";
    for (const auto& pair : dataMap) {
        columnNames = columnNames + pair.first + ", ";
        columnValues = columnValues + pair.second + "', '";
    }
    if (columnNames.length() >= 2) {
        columnNames.erase(columnNames.length() - 2);
        columnValues.erase(columnValues.length() - 3);
    }
    char* errorMessage;
    std::string insertQuery = "INSERT INTO records (" + columnNames + ") VALUES (" + columnValues + ");";
    int result = sqlite3_exec(db, insertQuery.c_str(), nullptr, nullptr, &errorMessage);
    if (result != SQLITE_OK) {
        std::cerr << "Error inserting values: " << errorMessage << std::endl;
        sqlite3_free(errorMessage);
    }
}


int printNumberRows(sqlite3* db, std::string tableName) {
    std::string countQuery = "SELECT COUNT(*) FROM records;";
    sqlite3_stmt* statement;
    int result = sqlite3_prepare_v2(db, countQuery.c_str(), -1, &statement, nullptr);
    if (result != SQLITE_OK) {
        std::cerr << "Error preparing count query: " << sqlite3_errmsg(db) << std::endl;
        return -1;
    }
    // Fetch the result
    int rowCount = 0;
    result = sqlite3_step(statement);
    if (result == SQLITE_ROW) rowCount = sqlite3_column_int(statement, 0);
    sqlite3_finalize(statement);
    return rowCount;
}


int main() {
    sqlite3* db = nullptr;
    int result = sqlite3_open("history.db", &db);
    if (result != SQLITE_OK) {
        std::cerr << "Error opening DB: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }
    std::cout << "DB opened successfully\n";

    std::string cityName = "berlin";
    std::string tableName = "records";
    bool initialization = initializeTable(db, tableName);
    if (initialization) {
        while (true) {
            std::string weatherData = getWeatherData(cityName);
            std::string coordinatesD;
            std::string conditionD;
            std::string timeD;
            extractValues(weatherData, coordinatesD, conditionD, timeD);
            if (!weatherData.empty()) {
                saveWeatherToFile("weatherOutput.txt", coordinatesD, conditionD, timeD);
            }
            std::unordered_map<std::string, std::string> insertMap;
            insertMap.insert({ "time", timeD });
            insertMap.insert({ "coordinates", coordinatesD });
            insertMap.insert({ "condition", conditionD });
            insertValuesIntoTable(db, tableName, insertMap);
            int numRows = printNumberRows(db, tableName);
            std::cout << "Current number of entries: " << numRows << "\n";
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }

    sqlite3_close(db);
    std::cout << "FINISHED\n";
    return 0;
}
