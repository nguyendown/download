/*
./download --url=https://localhost/a.iso --thread=1 --conn=1 --out=/tmp/a.iso
cmake -B build .
cmake --build build && build/main --url=http://192.168.12.1/a.exe --out=a.exe --thread 2
./build/main
 */

#include <iostream>
#include <thread>
#include <vector>
#include <fstream>
#include <filesystem>
#include <getopt.h>
#include <cstdint>
#include <curl/curl.h>

static size_t cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
    std::ofstream *of = static_cast<std::ofstream *>(userdata);
    size_t nbytes = size * nmemb;
    of->write(ptr, nbytes);
    return nbytes;
}

void my_curl_download(CURL *curl_new, std::string out, int64_t r_begin, int64_t r_end) {
    CURL *curl = curl_easy_duphandle(curl_new);
    std::string range;
    if (curl == 0)
        return;
    std::ofstream of(out, std::ios::binary);
    if (!of.is_open())
        return;
    //std::cout << std::filesystem::file_size(out) << '\n';
    range = std::to_string(r_begin) + '-' + std::to_string(r_end);
    //std::cout << range << '\n';
    curl_easy_setopt(curl, CURLOPT_RANGE, range.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &of);
    curl_easy_perform(curl);
    of.close();
    curl_easy_cleanup(curl);
}

struct option longopts[] = {
   { "url", required_argument, 0, 'u'},
   { "thread", required_argument, 0, 't'},
   { "conn", required_argument, 0, 'c'},
   { "out", required_argument, 0, 'o'},
   { 0, 0, 0, 0 }
};

int main(int argc, char *argv[]) {
    std::string url, out, out_part;
    std::ofstream of;
    int thread, conn, opt;
    CURL *curl_new, *curl;
    long res;
    curl_off_t cl;

    thread = 1;
    opt = 0;
    while((opt = getopt_long(argc, argv, "u:t:c:o:", longopts, NULL)) != -1) {
        switch(opt) {
        case 'u':
            url = optarg;
            break;
        case 't':
            thread = std::atoi(optarg);
            break;
        case 'c':
            conn = std::atoi(optarg);
            break;
        case 'o':
            out = optarg;
            break;
        }
    }

    if (url.empty() || out.empty())
        return 0;

    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_new = curl_easy_duphandle(curl);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
    curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &cl);
    if (cl == -1)
        return 1;

    curl_easy_setopt(curl, CURLOPT_RANGE, "0-1");
    curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &res);

    if (res != 206 || thread == 1) {
        of.open(out, std::ios::binary);
        if (!of.is_open())
            return 1;
        curl_easy_setopt(curl_new, CURLOPT_WRITEFUNCTION, cb);
        curl_easy_setopt(curl_new, CURLOPT_WRITEDATA, &of);
        curl_easy_perform(curl_new);
        curl_easy_cleanup(curl_new);
        of.close();
        return 0;
    }

    curl_easy_setopt(curl_new, CURLOPT_WRITEFUNCTION, cb);

    std::vector<std::thread> ts;

    int64_t r_size = cl / thread;
    int64_t r_begin = 0;
    int64_t r_end = r_begin + r_size;

    for (int i = 0; i < thread; i++) {
        //std::cout << cl << ' ' << r_size << ' ' << r_begin << ' ' << r_end << '\n';
        out_part = out + ".part" + std::to_string(i);
        ts.push_back(std::thread(my_curl_download, curl_new, out_part, r_begin, r_end));
        r_begin = r_end + 1;
        r_end = r_begin + r_size;
        if (r_end > cl)
            r_end = cl;
    }

    for (auto& t: ts) {
        t.join();
    }

    of.open(out, std::ios::binary);
    if (!of.is_open())
        return 1;

    for (int i = 0; i < thread; i++) {
        out_part = out + ".part" + std::to_string(i);
        std::ifstream is(out_part, std::ios::binary);
        of << is.rdbuf();
        is.close();
    }

    std::cout << "i got here\n";
}
