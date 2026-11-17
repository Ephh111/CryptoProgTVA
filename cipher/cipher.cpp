#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include <cryptopp/files.h>
#include <cryptopp/filters.h>
#include <cryptopp/osrng.h>
#include <cryptopp/pwdbased.h>
#include <cryptopp/sha.h>

using namespace CryptoPP;

static void printUsage(const char* prog)
{
    std::cout
        << "Использование:\n"
        << "  " << prog << " -e|-d -i <infile> -o <outfile> -p <password>\n\n"
        << "Параметры:\n"
        << "  -e           зашифровать\n"
        << "  -d           расшифровать\n"
        << "  -i <file>    входной файл\n"
        << "  -o <file>    выходной файл\n"
        << "  -p <pass>    пароль (для выработки ключа)\n"
        << "  -h           эта справка\n\n"
        << "Алгоритм: AES-256, режим CBC.\n";
}

static void derive(const std::string& pass, const byte* salt,
                   byte* key, byte* iv)
{
    byte buf[48];
    PKCS5_PBKDF2_HMAC<SHA256>().DeriveKey(
        buf, 48, 0, (const byte*)pass.data(), pass.size(), salt, 16, 100000);
    std::memcpy(key, buf, 32);
    std::memcpy(iv,  buf + 32, 16);
}

int main(int argc, char* argv[])
{
    if (argc == 1) { printUsage(argv[0]); return 1; }

    std::string mode, in, out, pass;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if      (a == "-h" || a == "--help") { printUsage(argv[0]); return 0; }
        else if (a == "-e" || a == "-d")     mode = a;
        else if (a == "-i" && i + 1 < argc)  in   = argv[++i];
        else if (a == "-o" && i + 1 < argc)  out  = argv[++i];
        else if (a == "-p" && i + 1 < argc)  pass = argv[++i];
        else {
            std::cerr << "Неизвестный аргумент: " << a << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    if (mode.empty() || in.empty() || out.empty() || pass.empty()) {
        std::cerr << "Не все обязательные параметры заданы\n";
        printUsage(argv[0]);
        return 1;
    }

    byte salt[16], key[32], iv[16];

    try {
        if (mode == "-e") {
            AutoSeededRandomPool prng;
            prng.GenerateBlock(salt, 16);
            derive(pass, salt, key, iv);

            std::ofstream ofs(out, std::ios::binary);
            if (!ofs) throw std::runtime_error("Не удалось открыть файл: " + out);
            ofs.write((char*)salt, 16);

            CBC_Mode<AES>::Encryption enc;
            enc.SetKeyWithIV(key, 32, iv);
            FileSource(in.c_str(), true,
                new StreamTransformationFilter(enc, new FileSink(ofs)));
        }
        else {
            std::ifstream ifs(in, std::ios::binary);
            if (!ifs) throw std::runtime_error("Не удалось открыть файл: " + in);
            ifs.read((char*)salt, 16);
            derive(pass, salt, key, iv);

            CBC_Mode<AES>::Decryption dec;
            dec.SetKeyWithIV(key, 32, iv);
            FileSource(ifs, true,
                new StreamTransformationFilter(dec, new FileSink(out.c_str())));
        }
    }
    catch (const Exception& e) {
        std::cerr << "Ошибка Crypto++: " << e.what() << "\n";
        return 1;
    }
    catch (const std::exception& e) {
        std::cerr << "Ошибка: " << e.what() << "\n";
        return 1;
    }
    
    if (mode == "-e") {
        std::cout << "Зашифрование завершено: " << out << std::endl;
    } else {
        std::cout << "Расшифрование завершено: " << out << std::endl;
    }
    
    return 0;
}
