#include<fstream>
#include<iostream>
#include<filesystem>
#include<vector>
#include<memory>
#include<string_view>
#include<cstring>
#include<cstdlib>

/**
 * Author:      Eustrain Lee
 */

constexpr int FILE_INDEX_ {1};
constexpr int PATTERN_INDEX_ {2};
constexpr int MODE_INDEX_ {3};

enum class SearchMode {
    all,
    file_name,
    content
};

template <typename ChTy>
::std::string&& stringToCharString(::std::basic_string<ChTy>&& str) {
    if constexpr (::std::is_same<ChTy,char>::value) {
        return ::std::forward<::std::string>(str);
    } else {
        ::std::basic_string<ChTy>& ws = str;
        std::string curLocale = setlocale(LC_ALL, nullptr);
        setlocale(LC_ALL, "chs");
        const wchar_t* Source = ws.c_str();
        size_t d_size = 2 * ws.size() + 1;
        char *dest = new char[d_size];
        memset(dest,0,d_size);
        wcstombs(dest,Source,d_size);
        std::string result = dest;
        delete []dest;
        setlocale(LC_ALL, curLocale.c_str());
        return ::std::move(result);
    }
}

//在指定目录项中查找，并将结果传递给输出流和错误流
extern int searchInDirectory (const ::std::filesystem::path& path,
                         ::std::string_view pattern,
                         ::SearchMode search_mode,
                         ::std::ostream& out,
                         ::std::ostream& err);

//主函数
int main(int argc, char* argv[]) {
    //读取文件路径
    if(argc <= FILE_INDEX_) {//没写文件路径
        ::std::cerr << "Enter a folder path, please." << ::std::endl;
        return -1 * FILE_INDEX_;
    }
    ::std::filesystem::path file_path {argv[FILE_INDEX_]};
    //读取模式串
    if(argc <= PATTERN_INDEX_) {//没写模式串
        ::std::cerr << "Enter a string of you will search." << ::std::endl;
        return -1 * PATTERN_INDEX_;
    }
    ::std::string pattern {argv[PATTERN_INDEX_]};
    //判断查找模式
    ::SearchMode search_mode = {::SearchMode::all};
    if(argc > MODE_INDEX_) {
        ::std::string search_mode_str {argv[MODE_INDEX_]};
        if(search_mode_str == "all");
        else if (search_mode_str == "filename") {
            search_mode = SearchMode::file_name;
        } else if (search_mode_str == "content") {
            search_mode = SearchMode::content;
        } else {
            ::std::cerr << "Error search mode." << ::std::endl;
            ::std::cerr << "Possible inputs: all, filename, content" << ::std::endl;
            return -1 * MODE_INDEX_;
        }
    }
    //记录查询是否有结果
    size_t res_num {0};
    //读取目录项
    //如果读取的是一个文件夹，那么读取文件夹中所有的目录项
    if(::std::filesystem::is_directory(file_path)) {
        //如果是文件夹则查询子文件夹/文件,统计结果并返回
        ::std::filesystem::recursive_directory_iterator iter {file_path};
        while(iter != ::std::filesystem::end(iter)) {
            res_num += searchInDirectory(iter->path(), pattern, search_mode, ::std::cout, ::std::cerr);
            ++iter;
        }
        /*
         * 关于为什么不使用for each
         * begin 函数存在于::std::filesystem名字空间中，
         * 若使用for each则需要导入名字空间但这并不会简化多少代码
         * 直接使用迭代器反而更直观
         */
    } else {//若不是文件夹, 直接读取数据
        res_num = searchInDirectory(file_path, pattern, search_mode, ::std::cout, ::std::cerr);
    }
    //打印结果
    if(res_num == 0) {
        ::std::cerr << "Not found string: " << pattern << "." << ::std::endl;
    } else {
        ::std::cout << "All files a total of " << res_num << " results." << ::std::endl;
    }
    return 0;
}

int searchInDirectory (const ::std::filesystem::path& path,
                        ::std::string_view pattern,
                        ::SearchMode search_mode,
                        ::std::ostream& out,
                        ::std::ostream& err) {
    //记录查询的结果数
    size_t res_num {0};
    //如果路径对应一个目录，那么对路径进行判断后返回
    if(::std::filesystem::is_directory(path)) {
        if (search_mode == SearchMode::file_name ||
            search_mode == SearchMode::all){
            //查询文件夹名末尾是否包含模式串
            ::std::string path_str {stringToCharString(path.filename().string())};
            if(path_str.find(pattern) != ::std::string::npos) {
                out << "In directory: " << path << ":" << ::std::endl;
                out << "Find a result: directory name " << path << ::std::endl;
                ++res_num;
            }
        }
        return res_num;
    }
    //打开文件
    ::std::ifstream ifs {path,::std::ios::binary};
    //检查文件是否已打开
    if(!ifs.is_open()) {
        err << "Open the file failed. Path: " << path << ::std::endl;
        return 0;
    }
    //自动释放文件
    struct FileCloser {
        void operator()(::std::ifstream* p_ifs){
            p_ifs->close();
        }
    };
    ::std::unique_ptr<::std::ifstream,FileCloser> o_o {&ifs};
    if( search_mode == SearchMode::file_name ||
        search_mode == SearchMode::all) {
        //查询文件名 中是否包含模式串
        ::std::string path_str {stringToCharString(path.filename().string())};
        if(path_str.find(pattern) != ::std::string::npos) {
            out << "In file: " << path << ":" << ::std::endl;
            out << "Find a result: file name " << path << ::std::endl;
            ++res_num;
        }
    }
    if( search_mode == SearchMode::content ||
        search_mode == SearchMode::all) {
        //保留空白
        ifs.unsetf(::std::ios_base::skipws);
        //模式串hash
        int64_t hash_key{0};
        //匹配串hash
        int64_t hash_str{0};
        //key串长度
        size_t pattern_len{0};
        //结果字符串的起始位置
        size_t res_pos{1};
        //结果字符串的所在的行
        size_t res_line{1};
        //结果字符串所在的列
        size_t res_col{1};
        //计算key的长度，同时求得hash_key
        for (const char *pc = &pattern[0];
             *pc != '\0';
             ++pattern_len,
             hash_key += static_cast<int64_t>(*(pc++)));
        //构建一个长度为key_len的定长循环队列，保存读过的key_len个字节
        ::std::vector<char> buff(pattern_len);
        //队首下标
        size_t buff_head{0};
        size_t len{0};
        //对'\r'进行优化
        bool passed_r = false;
        //从文件中读取数据
        while (!ifs.eof()) {
            char c;
            ifs >> c;
            if (len < pattern_len) {//读取前key_len个字节，存储到队列中
                hash_str += static_cast<int64_t>(c);
                buff[len++] = c;
            } else {//开始尝试匹配
                //相等判断
                if (hash_key == hash_str) {
                    bool is_equal = true;
                    for (size_t i = 0; i < pattern_len; ++i) {
                        if (buff[(buff_head + i) % pattern_len] != pattern[i]) {
                            is_equal = false;
                            break;
                        }
                    }
                    if (is_equal) {
                        if (res_num == 0) {
                            out << "In file: " << path << ":" << ::std::endl;
                        }
                        out << "Find a result:  ";
                        out << "line:  " << res_line << "\t";
                        out << "col:  " << res_col << "  \t";
                        out << "pos:  " << res_pos << ::std::endl;
                        ++res_num;
                    }
                }
                //更新记录的模式串位置
                //对Mac系统的换行优化
                if (passed_r) {
                    passed_r = false;
                    if (c == '\n') {
                        ifs >> c;
                    }
                }
                if (c == '\r') {
                    c = '\n';
                    passed_r = true;
                }
                if (buff[buff_head] == '\n') {
                    ++res_line;
                    res_col = 0;
                }
                ++res_col;
                ++res_pos;
                //更新hash值
                hash_str += static_cast<int64_t>(c);
                hash_str -= static_cast<int64_t>(buff[buff_head]);
                buff[buff_head] = c;
                //缓冲队列头下标右移
                buff_head = buff_head == pattern_len - 1 ? 0 : buff_head + 1;
            }
        }
        if (res_num != 0) {
            out << "A total of " << res_num << " results." << ::std::endl;
        }
    }
    return res_num;
}