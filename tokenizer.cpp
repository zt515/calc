#include "tokenizer.h"
#include "operator.h"
#include "keyword.h"
#include <cctype>

namespace kiva {
    namespace parser {

        Tokenizer::Tokenizer(const String &src)
            :originalSource(src), src(src.c_str())
        {
            orig = this->src;
        }

        Tokenizer::~Tokenizer()
        {}

        void Tokenizer::skipUntil(int end)
        {
            while (*src && *src != end) {
                ++src;
            }
        }

        bool Tokenizer::isIndentifierName(int token, bool first)
        {
            int is = (token >= 'a' && token <= 'z')
                     || (token >= 'A' && token <= 'Z')
                     || (token == '_');
            return first ? is : (is || (token >= '0' && token <= '9'));
        }

        char Tokenizer::peek() const
        {
            return *src;
        }

        char Tokenizer::peekChar() const
        {
            const char *p = src;
            while (std::isspace(*p)) {
                p++;
            }
            return *p;
        }

        String Tokenizer::duplicateFromHere() const
        {
            return String(src);
        }

        const char* Tokenizer::currentPosition() const
        {
            return src;
        }

        bool Tokenizer::next(Token &t, bool peek)
        {
            using namespace std;
            t.strval.clear();
            t.numval = 0;
            t.token = 0;

            const char *peekSrc = src;
            const char *&src = peek ? peekSrc : this->src;

            while ((t.token = *src++)) {
                if (isspace(t.token)) {
                    continue;
                }

                if (t.token >= '0' && t.token <= '9') {
                    Real token_val = t.token - '0';
                    // 四种数字：十进制(123) 十六进制(0x123) 八进制(017) 二进制(0b11)
                    if (token_val > 0) {
                        // 10进制，以 [1-9] 开始
                        // 仅支持10进制数中出现小数点
                        bool floating = false;
                        Real fpart = 0;
                        int nfpart = 1;

                        while (*src) {
                            if (*src >= '0' && *src <= '9') {
                                if (floating) {
                                    // 分开计算小数部分，最后将小数部分和整数部分相加
                                    fpart = fpart * 10 + *src++ - '0';
                                    nfpart *= 10;

                                } else {
                                    token_val = token_val * 10 + *src++ - '0';
                                }
                            } else if (*src == '.') {
                                floating = true;
                                src++;
                            } else {
                                break;
                            }
                        }

                        token_val += fpart / nfpart;

                    } else {
                        // 其它以0开始的
                        if (*src == 'x' || *src == 'X') {
                            // 十六进制
                            t.token = *++src;
                            while ((t.token >= '0' && t.token <= '9')
                                   || (t.token >= 'a' && t.token <= 'f')
                                   || (t.token >= 'A' && t.token <= 'F')) {
                                token_val = token_val * 16
                                            + (t.token & 15)
                                            + (t.token >= 'A' ? 9 : 0);
                                t.token = *++src;
                            }
                        } else if (*src == 'b' || *src == 'B') {
                            // 二进制
                            src++;
                            while (*src >= '0' && *src <= '1') {
                                token_val = token_val * 2 + *src++ - '0';
                            }
                        } else {
                            // 八进制
                            while (*src >= '0' && *src <= '7') {
                                token_val = token_val * 8 + *src++ - '0';
                            }
                        }
                    }

                    t.numval = token_val;
                    t.token = NUMBER;
                    return true;

                } else if (isIndentifierName(t.token, true)) {
                    t.strval += t.token;
                    while (isIndentifierName(*src, false)) {
                        t.strval += *src++;
                    }

                    int kw = Keyword::constantify(t.strval);
                    t.token = kw == -1 ? ID : kw;
                    return true;

                } else if (t.token == '"') {
                    size_t count = 0;
                    const char *start = src;
                    while (*src && *src != t.token) {
                        if (*src == '\\') {
                            ++src;
                            ++count;
                        }
                        ++count;
                        ++src;
                    }
                    // 跳过结尾的引号
                    src++;

                    t.token = STRING;
                    t.strval = String(start, count);
                    return true;

                } else if (t.token == '+') {
                    t.token = ADD;
                    return true;

                } else if (t.token == '-') {
                    // 减号或者负号
                    const char *p = src - 2;
                    if (p < orig /* 如果可以等于，会出现误判的情况 */
                            || Operator::isOperator(*p)
                            || *p == '(') {
                        t.token = NAV;
                    } else {
                        t.token = SUB;
                    }
                    return true;

                } else if (t.token == '*') {
                    t.token = MUL;
                    return true;

                } else if (t.token == '/') {
                    t.token = DIV;
                    return true;

                } else if (t.token == '^') {
                    t.token = XOR;
                    return true;

                } else if (t.token == '|') {
                    // || 或 |
                    if (*src == '|') {
                        src++;
                        t.token = LOR;
                    } else {
                        t.token = OR;
                    }
                    return true;

                } else if (t.token == '&') {
                    // && 或 &
                    if (*src == '&') {
                        src++;
                        t.token = LAND;
                    } else {
                        t.token = AND;
                    }
                    return true;

                } else if (t.token == '<') {
                    // << 或 <= 或 <
                    if (*src == '<') {
                        src++;
                        t.token = LSHF;
                    } else if (*src == '=') {
                        src++;
                        t.token = LE;
                    } else {
                        t.token = LT;
                    }
                    return true;

                } else if (t.token == '>') {
                    // >> 或 >= 或 >
                    if (*src == '>') {
                        src++;
                        t.token = RSHF;
                    } else if (*src == '=') {
                        src++;
                        t.token = GE;
                    } else {
                        t.token = GT;
                    }
                    return true;

                } else if (t.token == '%') {
                    t.token = MOD;
                    return true;

                } else if (t.token == '=') {
                    // = 或 ==
                    if (*src == '=') {
                        src++;
                        t.token = EQ;
                    } else {
                        t.token = ASSIGN;
                    }
                    return true;

                } else if (t.token == '!') {
                    // ! 或 !=
                    if (*src == '=') {
                        src++;
                        t.token = NEQ;
                    } else {
                        t.token = NOT;
                    }
                    return true;

                } else if (t.token == ';'
                           || t.token == '{'
                           || t.token == '}'
                           || t.token == '('
                           || t.token == ')'
                           || t.token == ',') {
                    // 将这些符号当成标记
                    return true;
                }
            }

            return false;
        }

        void Tokenizer::match(Token &t, int token) throw(std::runtime_error)
        {
            if (t.token == token) {
                next(t);
            } else {
                throw std::runtime_error("Unexpected token");
            }
        }
    }
}

