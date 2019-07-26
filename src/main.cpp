/**
 * Основной сервер для "умного устройства".
 *
 * Должен подключаться к WiFi сети, если не удастся, то создать свою.
 * Должен обладать лёгким веб-интерфейсом на основе Bootstrap.
 *
 *
 *
 * Источники
 * http://esp8266-arduinoide.ru/step1-wifi/             ШАГ №1 — WI-FI ПОДКЛЮЧЕНИЕ
 * http://esp8266-arduinoide.ru/step2-webserver/        ШАГ №2 — WEBSERVER WEB СЕРВЕР
 * http://esp8266-arduinoide.ru/step3-ssdp/             ШАГ №3 — SSDP ОБНАРУЖЕНИЕ
 * http://esp8266-arduinoide.ru/step4-fswebserver/      ШАГ №4 — FSWEBSERVER
 * http://esp8266-arduinoide.ru/step5-datapages/        ШАГ №5 — ПЕРЕДАЧА ДАННЫХ НА WEB СТРАНИЦУ
 * http://esp8266-arduinoide.ru/step6-datasend/         ШАГ №6 — ПЕРЕДАЧА ДАННЫХ C WEB СТРАНИЦЫ
 * http://esp8266-arduinoide.ru/step7-fileconfig/       ШАГ №7 — ЗАПИСЬ И ЧТЕНИЕ ПАРАМЕТРОВ КОНФИГУРАЦИИ В ФАЙЛ
 * http://esp8266-arduinoide.ru/step8-timeupdate/       ШАГ №8 — WEB ОБНОВЛЕНИЕ, ВРЕМЯ ИЗ СЕТИ.
 * http://esp8266-arduinoide.ru/step9-codnohtml/        ШАГ №9 — СОЗДАНИЕ WEB СТРАНИЦ В ESP8266, БЕЗ ЗНАНИЙ HTML
 * http://esp8266-arduinoide.ru/step10-datanohtml/      ШАГ №10 — СОЗДАНИЕ WEB СТРАНИЦ В ESP8266, ДАННЫЕ НА СТРАНИЦУ
 * http://esp8266-arduinoide.ru/step11-grafnohtml/      ШАГ №11 — СОЗДАНИЕ WEB СТРАНИЦ В ESP8266, ГРАФИКИ
 * http://esp8266-arduinoide.ru/step12-graf-dht/        ШАГ №12 — ESP8266 ТЕМПЕРАТУРА И ВЛАЖНОСТЬ НА ГРАФИКЕ
 * http://esp8266-arduinoide.ru/step13-tickerscheduler/ ШАГ №13 — ESP8266 БИБЛИОТЕКА TICKERSCHEDULER
 *
 * https://github.com/tretyakovsa/Sonoff_WiFi_switch/wiki/Возможности-page.htm%3F*
 *
 * http://www.youtube.com/user/Renat2985/ - Канал Рената К
 *
 */
#include <Arduino.h>        // Обязательная библиотека, поскольку программировалось в VSC.
#include <WiFi.h>           // https://www.arduino.cc/en/Reference/WiFi
#include <WebServer.h>
#include <ESP32SSDP.h>      // https://github.com/luc-github/ESP32SSDP
#include <FS.h>
#include <SPIFFS.h>         // Библиотека для работы с файловой системой. Входит в пакет. Замена ардуиновской FS-библиотеке. Туториал https://techtutorialsx.com/2019/02/23/esp32-arduino-list-all-files-in-the-spiffs-file-system/
#include <ArduinoJson.h>    // Установить из менеджера библиотек. Сайт https://arduinojson.org (Устанавливался через менеджер библиотек Platformio. Но есть и https://github.com/bblanchon/ArduinoJson.git )
//#include <HTTPUpdate.h>   //Не думаю, что мне пока понадобится обновление "на лету"
#include <time.h>           //Содержится в пакете

IPAddress apIP(192, 168, 4, 1);         /* Ай-Пи адрес устройства, в режиме точки доступа */

// Определяем значения по умолчанию
#define WiFi_SSID "Home"                /* SSID сети, к которой мы подключаемся */
#define WiFi_password "HomePassword"    /* пароль для подключения к сети */
#define WiFi_AP_SSID "WiFi"             /* SSID AP точки доступа */
#define WiFi_AP_password "i2345678"     /* пароль точки доступа */
#define WiFi_trying_to_connect  11      /* кол-во попыток подсоединиться к сети */

#define SSDP_Name "Esp32"               /* Имя для обозначения в сетевых устройствах */
#define timezone 3                      /* часовой пояс GTM */

/**
 * Глобальная переменная для хранения настроек в JSON-формате.
 * Поля, которые будут храниться:
 *  1. SSDPName
 *  2. ssidAPName
 *  3. ssidAPPassword
 *  4. ssidName
 *  5. ssidPassword
 *  6. timezone
 */ 
String jsonConfig = "{}";

// Web интерфейс для устройства
// WiFiServer HTTP(80);
WebServer HTTP(80);

// Для файловой системы
File fsUploadFile;






/* 6. синхронизация времени */

/**
 * Получение текущего времени
 */
String GetTime() {
 time_t now = time(nullptr); // получаем время с помощью библиотеки time.h
 String Time = ""; // Строка для результатов времени
 Time += ctime(&now); // Преобразуем время в строку формата Thu Jan 19 00:55:35 2017
 int i = Time.indexOf(":"); //Ишем позицию первого символа :
 Time = Time.substring(i - 2, i + 6); // Выделяем из строки 2 символа перед символом : и 6 символов после
 return Time; // Возврашаем полученное время
}


// Получение даты
String GetDate() {
 time_t now = time(nullptr); // получаем время с помощью библиотеки time.h
 String Data = ""; // Строка для результатов времени
 Data += ctime(&now); // Преобразуем время в строку формата Thu Jan 19 00:55:35 2017
 int i = Data.lastIndexOf(" "); //Ишем позицию последнего символа пробел
 String Time = Data.substring(i - 8, i+1); // Выделяем время и пробел
 Data.replace(Time, ""); // Удаляем из строки 8 символов времени и пробел
 return Data; // Возврашаем полученную дату
}


/**
 * Подключение к NTP-серверам и подключение даты и времени с них.
 */
void timeSynch(int zone){
    if (WiFi.status() == WL_CONNECTED) {
        // Настройка соединения с NTP сервером
        configTime(zone * 3600, 0, "pool.ntp.org", "ru.pool.ntp.org");
        int i = 0;
        Serial.println("\nWaiting for time");
        delay(1000);
        while (!time(nullptr) && i < 10) {
            Serial.print(".");
            i++;
            delay(1000);
        }
        Serial.println("");
        Serial.println("ITime Ready!");
        Serial.println(GetTime());
        Serial.println(GetDate());
    }
}


/**
 * Определение длины одноуровнего JSON.
 * На самом деле оно считает сколько раз в строчке json встречается <<",>>
 */
int getJSONLength(String json) {
    // Вначале проверим не пустой ли json.
    if (json.indexOf("\"") == -1)
        return 0;

    // json состоит минимум из одного элемента
    // счётчик элементов
    int count = 0;
    // первое нахождение разделителя
    int from = json.indexOf("\",", 0);

    while (from >= 0) {
        // Увеличиваем счётчик
        count++;

        // Ищем следующий разделитель
        from = json.indexOf("\",", from+1);
    }

    // Возвращаем кол-во находок, не забывая увеличить значение на один.
    return (count+1);
}


// ------------- Чтение значения json
String jsonRead(String &json, String name) {
    /*
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(json);
    return root[name].as<String>();
    */
    // Резервируем память для json объекта.
    const int capacity = JSON_OBJECT_SIZE(getJSONLength(json));
    DynamicJsonDocument doc(capacity);

    Serial.println("-------");
    Serial.println(json);
    Serial.println(doc.size());
    Serial.println("-------");

    // "Десерилизация" - перевот строчки в JSON-объект
    DeserializationError err = deserializeJson(doc, json);

    if (err) {
        Serial.print(F("jsonRead.deserializeJson() failed with code "));
        Serial.println(err.c_str());
        return ""; // В случае ошибки будет возвращаться пустую строчку
        //return false;
    }

    return doc[name];
}


// ------------- Запись значения json String
String jsonWrite(String &json, String name, String volume) {
    /*
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(json);
    root[name] = volume;
    json = "";
    root.printTo(json);
    return json;
    */
   
    // Резервируем память для json объекта.
    // DynamicJsonBuffer jsonBuffer;
    const int capacity = JSON_OBJECT_SIZE(getJSONLength(json)+10);
    DynamicJsonDocument doc(capacity);

    // "Десерилизация" - перевот строчки в JSON-объект
    DeserializationError err = deserializeJson(doc, json);

    if (err) {
        Serial.print(F("jsonWrite.deserializeJson() failed with code "));
        Serial.println(err.c_str());
        return "";
    }

    // Обновляем поле
    doc[name] = volume;

    // Обнуляем оригинальную переменную json
    json = "";
    // Помещаем созданный doc в json
    serializeJson(doc, json);

    return (json);
}


String jsonWrite(String &json, String name, int value) {
    return jsonWrite(json, name, String(value));
}



/* 5. работа с конфигурационным файлом */

/**
 * Сохраняем в глобальной переменной значения по умолчанию.
 */
String setDefaultValuesToJSONConfig () {
    // Резервируем память для json объекта.
    // На всякий случай памяти выделим побольше
    const int capacity = JSON_OBJECT_SIZE(getJSONLength(jsonConfig)+10);
    // DynamicJsonBuffer jsonBuffer;
    DynamicJsonDocument json(capacity);

    //Serial.print("подсчитанная величина: ");
    //Serial.println(capacity);

    // вызовите парсер JSON через экземпляр jsonBuffer
    // строку возьмем из глобальной переменной String jsonConfig
    // JsonObject& root = jsonBuffer.parseObject(jsonConfig);
    // "Десерилизация" - перевод строчки в JSON-объект
    DeserializationError err = deserializeJson(json, jsonConfig);
    if (err) {
        Serial.print(F("setDefaultValuesToJSONConfig.deserializeJson() failed with code "));
        Serial.println(err.c_str());
        return "";
    }

    //Serial.print("Размер переменной json: ");
    //Serial.println(json.size());

    // Заполняем поля json
    json["SSDPName"] = SSDP_Name;
    json["ssidAPName"] = WiFi_AP_SSID;
    json["ssidAPPassword"] = WiFi_AP_password;
    json["ssidName"] = WiFi_SSID;
    json["ssidPassword"] = WiFi_password;
    json["timezone"] = timezone;

    //Serial.print("Размер переменной json: ");
    //Serial.println(json.size());

    // Обнуляем глобальную переменную jsonConfig
    jsonConfig = "";
    // Помещаем созданный json в jsonConfig
    // json.printTo(jsonConfig);
    serializeJson(json, jsonConfig);

    return jsonConfig;
}



/**
 * Запись настроек в файл config.json
 */
bool saveConfig() {
    Serial.println("Сохраняем конфигурационный файл.");

    // Непосредственно сохраняем данные.
    // Открываем файл для записи
    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
        Serial.println("Failed to open config file for writing");
        return false;
    }

    // Записываем строку json в файл 
    //json.printTo(configFile);
    configFile.print(jsonConfig);

    // Закрываем файл
    configFile.close();

    return true;
}


/**
 * Создаём новый конфигурационный файл.
 * Загружаем значения по умолчанию и записываем их в файл.
 */
bool createConfigFile() {
    // Загружаем данные по умолчанию в глобальную переменную конфигурации
    setDefaultValuesToJSONConfig();
    
    // Создаем файл записав в него даные по умолчанию
    return saveConfig();
}


/**
 * Загрузка данных сохраненных в файле config.json.
 * https://techtutorialsx.com/2018/08/05/esp32-arduino-spiffs-reading-a-file/
 */
bool loadConfig() {
    // Открываем файл для чтения
    File configFile = SPIFFS.open("/config.json", "r");

    // Проверка получилось ли открыть файл.
    if (!configFile) {
        // Закрываем файл
        configFile.close();

        Serial.println(F("loadConfig() Файл не найден."));

        return createConfigFile();
    } else if (!configFile.available()) {
        // Закрываем файл
        configFile.close();

        Serial.println(F("loadConfig() Файл пуст."));

        return createConfigFile();
    }

    // Проверяем размер файла, будем использовать файл размером меньше 1024 байта и больше 10 байт
    size_t size = configFile.size();
    if (size > 1024) {
        //@TODO Размер файла надо проверять до записи, а не после
        Serial.println(F("Файл конфигурации слишком большой"));
        
        // Закрываем файл
        configFile.close();

        return createConfigFile();
    } else if (size < 10) {
        Serial.println(F("Файл конфигурации слишком маленький"));
        
        // Закрываем файл
        configFile.close();

        return createConfigFile();
    }

    /* Последняя проверка конфигурационного файла, если там будет содержаться пустой JSON.
     * Теоретически это проверка излишня, поскольку размер файла уже проверялся.
     */
    String configFileContent = configFile.readString();
    if (configFileContent == "{}") {
        // Закрываем файл
        configFile.close();

        Serial.println(F("loadConfig() JSON пуст."));

        return createConfigFile();
    }

    // загружаем файл конфигурации в глобальную переменную
    jsonConfig = configFileContent;

    // Закрываем файл
    configFile.close();

    // Резервируем память для json объекта. Выделяем побольше памяти, иначе во время десериализации выскакивает ошибка о нехватке памяти.
    // DynamicJsonBuffer jsonBuffer;
    const int capacity = JSON_OBJECT_SIZE(getJSONLength(jsonConfig)+10);
    DynamicJsonDocument doc(capacity);

    // вызовите парсер JSON через экземпляр jsonBuffer
    // строку возьмем из глобальной переменной String jsonConfig
    // JsonObject& root = jsonBuffer.parseObject(jsonConfig);
    // "Десерилизация" - перевот строчки в JSON-объект
    DeserializationError err = deserializeJson(doc, jsonConfig);

    if (err) {
        Serial.print(F("loadConfig.deserializeJson() failed with code "));
        Serial.println(err.c_str());
        return false;
    }

    // Все данные только были помещены в jsonConfig, нет смысла их дублировать по разным переменным.
    //_ssidAP = root["ssidAPName"].as<String>(); // Так получаем строку
    //_passwordAP = root["ssidAPPassword"].as<String>();
    //timezone = root["timezone"];               // Так получаем число
    //SSDP_Name = root["SSDPName"].as<String>();
    //_ssid = root["ssidName"].as<String>();
    //_password = root["ssidPassword"].as<String>();

    return true;
}


/**
 * Установка параметров времянной зоны по запросу вида /TimeZone?timezone=3
 */
void handle_time_zone() {
    jsonWrite(
        jsonConfig,
        "timezone",
        HTTP.arg("timezone").toInt()); // Получаем значение timezone из запроса, конвертируем в int
    //timezone = HTTP.arg("timezone").toInt();
    saveConfig();
    HTTP.send(200, "text/plain", "OK");
}


void handle_Time(){
    timeSynch(timezone);
    HTTP.send(200, "text/plain", "OK"); // отправляем ответ о выполнении
}


void Time_init() {
    HTTP.on("/Time", handle_Time);     // Синхронизировать время устройства по запросу вида /Time
    HTTP.on("/TimeZone", handle_time_zone);    // Установка времянной зоны по запросу вида /TimeZone?timezone=3
    timeSynch(timezone);
}



/* 4. Файловая система */

/**
 * Отображает файлы в папке.
 * Пример: http://192.168.1.7/list?dir=/
 */
void handleFileList() {
    if (!HTTP.hasArg("dir")) {
        HTTP.send(500, "text/plain", "BAD ARGS");
        return;
    }

    String path = HTTP.arg("dir");

    /*
    Dir dir = SPIFFS.openDir(path);
    String output = "[";
    while (dir.next()) {
        File entry = dir.openFile("r");
        if (output != "[") output += ',';
        bool isDir = false;
        output += "{\"type\":\"";
        output += (isDir) ? "dir" : "file";
        output += "\",\"name\":\"";
        output += String(entry.name()).substring(1);
        output += "\"}";
        entry.close();
    }
    output += "]";
    */

    // https://techtutorialsx.com/2019/02/23/esp32-arduino-list-all-files-in-the-spiffs-file-system/

    File root = SPIFFS.open(path);
    File file = root.openNextFile();

    String output = "[";
    while(file){
        if (output != "[") output += ",\n";

        output += "{\"type\":\"";
        output += (file.isDirectory()) ? "dir" : "file";

        output += "\",\"name\":\"";
        output += String(file.name());

        output += "\",\"size\":\"";
        output += String(file.size());

        output += "\"}";

        file = root.openNextFile();
    }
    output += "]";

    HTTP.send(200, "text/json", output);

    // Сбрасываем переменную "пути"
    path = String();
}


/**
 * Выбор правильного "ContentType" файла.
 * Нужно, чтобы правильно отправить файл клиенту.
 */
String getContentType(String filename) {
    //Serial.println("Filename: " + filename + "\n");
    if (HTTP.hasArg("download")) return "application/octet-stream";
    else if (filename.endsWith(".htm")) return "text/html";
    else if (filename.endsWith(".html")) return "text/html";
    else if (filename.endsWith(".json")) return "application/json";
    else if (filename.endsWith(".css")) return "text/css";
    else if (filename.endsWith(".js")) return "application/javascript";
    else if (filename.endsWith(".png")) return "image/png";
    else if (filename.endsWith(".gif")) return "image/gif";
    else if (filename.endsWith(".jpg")) return "image/jpeg";
    else if (filename.endsWith(".ico")) return "image/x-icon";
    else if (filename.endsWith(".xml")) return "text/xml";
    else if (filename.endsWith(".pdf")) return "application/x-pdf";
    else if (filename.endsWith(".zip")) return "application/x-zip";
    else if (filename.endsWith(".gz")) return "application/x-gzip";
    return "text/plain";
}


/**
 * Открытие файла и отправка его клиенту (браузеру)
 */
bool handleFileRead(String path) {
    if (path.endsWith("/")) path += "index.htm";
    String contentType = getContentType(path);
    String pathWithGz = path + ".gz";
    if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {
        if (SPIFFS.exists(pathWithGz))
            path += ".gz";

        Serial.println("файл " + path + " найден \n");

        File file = SPIFFS.open(path, "r");
        HTTP.streamFile(file, contentType);
        file.close();
        return true;
    }

    Serial.println("Файл " + path + " НЕ найден. \n");

    return false;
}


/**
 * Создание файлов
 */
void handleFileCreate() {
    if (HTTP.args() == 0)
        return HTTP.send(500, "text/plain", "BAD ARGS");
    String path = HTTP.arg(0);
    if (path == "/")
        return HTTP.send(500, "text/plain", "BAD PATH");
    if (SPIFFS.exists(path))
        return HTTP.send(500, "text/plain", "FILE EXISTS");
    File file = SPIFFS.open(path, "w");
    if (file)
        file.close();
    else
        return HTTP.send(500, "text/plain", "CREATE FAILED");
    HTTP.send(200, "text/plain", "");
    path = String();
}


/**
 * Удаляем файл
 */
void handleFileDelete() {
    if (HTTP.args() == 0) return HTTP.send(500, "text/plain", "BAD ARGS");
    String path = HTTP.arg(0);
    if (path == "/")
      return HTTP.send(500, "text/plain", "BAD PATH");
    if (!SPIFFS.exists(path))
      return HTTP.send(404, "text/plain", "FileNotFound");
    SPIFFS.remove(path);
    HTTP.send(200, "text/plain", "");
    path = String();
}


/**
 * Загружаем файл
 */
void handleFileUpload() {
    if (HTTP.uri() != "/edit") return;
    HTTPUpload& upload = HTTP.upload();

    if (upload.status == UPLOAD_FILE_START) {
        String filename = upload.filename;
        if (!filename.startsWith("/")) filename = "/" + filename;
        fsUploadFile = SPIFFS.open(filename, "w");
        filename = String();
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        //DBG_OUTPUT_PORT.print("handleFileUpload Data: "); DBG_OUTPUT_PORT.println(upload.currentSize);
        if (fsUploadFile)
            fsUploadFile.write(upload.buf, upload.currentSize);
    } else if (upload.status == UPLOAD_FILE_END) {
        if (fsUploadFile)
            fsUploadFile.close();
    }
}


// Инициализация FFS
void FS_init(void) {
    SPIFFS.begin();
    /*
    {
        Dir dir = SPIFFS.openDir("/");
        while (dir.next()) {
            String fileName = dir.fileName();
            size_t fileSize = dir.fileSize();
        }
    }
    */

    //HTTP страницы для работы с FFS
    //list directory
    HTTP.on("/list", HTTP_GET, handleFileList);
    //загрузка редактора editor
    HTTP.on("/edit", HTTP_GET, []() {
        if (!handleFileRead("/edit.htm")) HTTP.send(404, "text/plain", "FileNotFound");
    });

    HTTP.on("/edit", HTTP_PUT, handleFileCreate);
    //Удаление файла
    HTTP.on("/edit", HTTP_DELETE, handleFileDelete);
    //first callback is called after the request has ended with all parsed arguments
    //second callback handles file uploads at that location
    HTTP.on("/edit", HTTP_POST, []() {
        HTTP.send(200, "text/plain", "");
    }, handleFileUpload);
    //called when the url is not defined here
    //use it to load content from SPIFFS
    HTTP.onNotFound([]() {
        //Serial.println("File not fount \n");
        if (!handleFileRead(HTTP.uri()))
            HTTP.send(404, "text/plain", "FileNotFound");
    });
}


/* 1. Start WIFI */

/**
 * Создаёт WiFi точку доступа 
 */
bool StartAPMode() {
    // Отключаем WIFI
    WiFi.disconnect();
    // Меняем режим на режим точки доступа
    WiFi.mode(WIFI_AP);
    // Задаем настройки сети
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    // Включаем WIFI в режиме точки доступа с именем и паролем
    // хронящихся в переменных _ssidAP _passwordAP
    WiFi.softAP(WiFi_AP_SSID, WiFi_AP_password);
    return true;
}


/**
 * Подключаемся к WiFi точке доступа.
 */
bool WiFiConnect() {
    // Попытка подключения к точке доступа
    WiFi.mode(WIFI_STA);
    byte tries = WiFi_trying_to_connect;
    WiFi.begin(WiFi_SSID, WiFi_password);
    while (--tries && WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(1000);
    }
    return true;
}


/**
 * Вначале пытается подключиться к WiFi сети,
 * если не удастся, то создаёт свою точку доступа.
 */
void WIFIinit() {
    // Подсоединяемся к точке доступа.
    WiFiConnect();

    Serial.println("");
    if (WiFi.status() != WL_CONNECTED) {
        // Если не удалось подключиться запускаем в режиме AP
        Serial.println("WiFi up AP 192.168.4.1");
        StartAPMode();
    } else {
        Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
    }
}


/* 2. Start WebServer */

// Ответ если страница не найдена
void handleNotFound() {
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += HTTP.uri();
    message += "\nMethod: ";
    message += (HTTP.method() == HTTP_GET)?"GET":"POST";
    message += "\nArguments: ";
    message += HTTP.args();
    message += "\n";
    for (uint8_t i=0; i<HTTP.args(); i++){
        message += " " + HTTP.argName(i) + ": " + HTTP.arg(i) + "\n";
    }
    HTTP.send(404, "text/plain", message);
}


/**
 * Ответ при обращении к основной странице.
 * Тест-функция для первых версий кода.
 * @TODO удалить
 */
void handleRoot() {
  HTTP.send(200, "text/plain", "hello from esp32!");
}


// Перезагрузка модуля по запросу вида http://192.168.1.7/restart?device=ok
void handle_Restart() {
    String restart = HTTP.arg("device");
    if (restart == "ok") ESP.restart();
    HTTP.send(200, "text/plain", "OK");
}


/**
 * Генерация JSON файла с настройками.
 * Пример строчки: {"SSDP":"SSDP-test","ssid":"home","password":"i12345678","ssidAP":"WiFi","passwordAP":"","ip":"192.168.0.101"}
 */
void handle_ConfigJSON() {
    // Резервируем память для json объекта. Поскольку к моменту компиляции мы будем знать размер и структуру JSON-объекта, то делаем его статичным.
    const int capacity = JSON_OBJECT_SIZE(9);
    StaticJsonDocument<capacity> doc;

    // Заполняем поля
    doc["SSDP"] = SSDP_Name; // Имя SSDP
    doc["ssidAP"] = WiFi_AP_SSID; // Имя точки доступа
    doc["passwordAP"] = WiFi_AP_password; // Пароль точки доступа
    doc["ssid"] = WiFi_SSID; // Имя сети
    doc["password"] = WiFi_password; // Пароль сети
    doc["timezone"] = timezone; // Часовой пояс
    doc["ip"] = WiFi.localIP().toString(); // IP устройства
    doc["time"] = GetTime(); // Время
    doc["date"] = GetDate(); // Дата

    // Помещаем созданный doc в переменную root
    String root = "";
    
    serializeJson(doc, root);
    HTTP.send(200, "text/json", root);
}


// Функции API-Set
// Установка SSDP имени по запросу вида /ssdp?ssdp=proba
void handle_Set_Ssdp() {
    jsonWrite(
        jsonConfig,
        "SSDPName",
        HTTP.arg("ssdp")); // Получаем значение ssdp из запроса
    saveConfig();
    HTTP.send(200, "text/plain", "OK");
}


/**
 * Установка параметров для подключения к внешней AP по запросу вида /ssid?ssid=home2&password=12345678
 */
void handle_Set_Ssid() {
    jsonWrite(
        jsonConfig,
        "ssidName",
        HTTP.arg("ssid")); // Получаем значение ssid из запроса
    jsonWrite(
        jsonConfig,
        "ssidPassword",
        HTTP.arg("password")); // Получаем значение password из запроса
    saveConfig();                        // Функция сохранения данных во Flash пока пустая
    HTTP.send(200, "text/plain", "OK");   // отправляем ответ о выполнении
}


/**
 * Установка параметров внутренней точки доступа по запросу вида /ssidap?ssidAP=home1&passwordAP=8765439
 */
void handle_Set_Ssidap() {
    jsonWrite(
        jsonConfig,
        "ssidAPName",
        HTTP.arg("ssidAP")); // Получаем значение ssidAP из запроса
    jsonWrite(
        jsonConfig,
        "ssidAPPassword",
        HTTP.arg("passwordAP")); // Получаем значение passwordAP из запроса
    saveConfig();                         // Функция сохранения данных во Flash пока пустая
    HTTP.send(200, "text/plain", "OK");   // отправляем ответ о выполнении
}


/**
 * Настройка и запуск веб-сервера.
 */
void HTTP_init(void) {
    // Настройка
    //HTTP.onNotFound(handleNotFound); // Сообщение если нет страницы. Попробуйте ввести http://192.168.1.7/restar?device=ok&test=1&led=on

    //HTTP.on("/", handleRoot); // Главная страница http://192.168.1.7/

    // -------------------построение графика по запросу вида /charts.json?data=A0&data2=stateLed
    HTTP.on("/charts.json", HTTP_GET, []() {
        String message = "{";                       // создадим json на лету
        uint8_t j = HTTP.args();                    // получим количество аргументов
        for (uint8_t i = 0; i < j; i++) { // Будем читать все аргументы по порядку
            String nameArg = HTTP.argName(i);         // Возьмем имя аргумента и зададим массив с ключём по имени аргумента      
            String keyArg = HTTP.arg(i);
            String value = jsonRead(jsonConfig, HTTP.arg(i)); // Считаем из configJson значение с ключём keyArg
            if (value != "")  { // если значение есть добавим имя массива
                message += "\"" + nameArg + "\":["; // теперь в строке {"Имя аргумента":[
                message += value; // добавим данные в массив теперь в строке {"Имя аргумента":[value
                value = "";       // очистим value 
            }
            message += "]"; // завершим массив теперь в строке {"Имя аргумента":[value]
            if (i<j-1) message += ","; // если элемент не последний добавит , теперь в строке {"Имя аргумента":[value],
        }
        message += "}";
        // теперь json строка полная
        jsonWrite(message, "points", 10); // зададим количество точек по умолчанию
        jsonWrite(message, "refresh", 1000); // зададим время обнавления графика по умолчанию
        HTTP.send(200, "application/json", message);
    });


    HTTP.on("/configs.json", handle_ConfigJSON); // формирование configs.json страницы для передачи данных в web интерфейс

    // API для устройства
    HTTP.on("/ssdp", handle_Set_Ssdp);     // Установить имя SSDP устройства по запросу вида /ssdp?ssdp=proba
    HTTP.on("/ssid", handle_Set_Ssid);     // Установить имя и пароль роутера по запросу вида /ssid?ssid=home2&password=12345678
    HTTP.on("/ssidap", handle_Set_Ssidap); // Установить имя и пароль для точки доступа по запросу вида /ssidap?ssidAP=home1&passwordAP=8765439
    
    Time_init();

    HTTP.on("/restart", handle_Restart); // Перезагрузка модуля по запросу вида http://192.168.1.7/restart?device=ok

    // Запускаем HTTP сервер
    HTTP.begin();
}


/* 3. Start SSDP */

/**
 * Настраивается и запускается SSDP.
 * При помощи этой службы
 * можно увидеть устройство в списке сетевых устройств.
 */
void SSDP_init(void) {
    // SSDP дескриптор
    HTTP.on("/description.xml", HTTP_GET, []() {
        SSDP.schema(HTTP.client());
    });
    //Если версия  2.0.0 закаментируйте следующую строчку
    SSDP.setDeviceType("upnp:rootdevice");
    SSDP.setSchemaURL("description.xml");
    SSDP.setHTTPPort(80);
    SSDP.setName(SSDP_Name);
    SSDP.setSerialNumber("001788102201");
    SSDP.setURL("/");
    SSDP.setModelName("SSDP-Test");
    SSDP.setModelNumber("000000000001");
    SSDP.setModelURL("https://github.com/login");
    SSDP.setManufacturer("Andrey LI");
    SSDP.setManufacturerURL("https://github.com/login");
    SSDP.begin();
}




/* Основная часть */

void setup() {
    Serial.begin(115200);
    Serial.println("");

    //Запускаем файловую систему
    Serial.println("Start 4-FS");
    FS_init();

    //Загружаем настройки
    Serial.println("Load FileConfig");
    loadConfig();

    //Запускаем WIFI
    Serial.println("Start 1-WIFI");
    WIFIinit();

    //Настраиваем и запускаем HTTP интерфейс
    Serial.println("Start 2-WebServer");
    HTTP_init();

    //Настраиваем и запускаем SSDP интерфейс
    Serial.println("Start 3-SSDP");
    SSDP_init();


    Serial.println("Настройка закончена.");
}

void loop() {
    // Обработка событий веб-клиента.
    HTTP.handleClient();
    delay(10);
}
