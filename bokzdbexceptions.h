#ifndef BOKZDBEXCEPTIONS_H
#define BOKZDBEXCEPTIONS_H
#include <QtConcurrent/QtConcurrent>

/*класс exception для Database*/
class DbException : public std::exception
{
public:
    DbException() noexcept: whatStr("Неизвестная ошибка") {}
    DbException(const std::string &&whatStr) noexcept : whatStr(std::move(whatStr)) { }
    DbException(const std::string &whatStr) noexcept : whatStr(whatStr) { }
    ~DbException() noexcept = default;

    const char* what() const noexcept override
    {
        return whatStr.c_str();
    }

private:
    std::string whatStr;
};


class ThreadDbException : public QtConcurrent::Exception
{
public:
    ThreadDbException(std::exception& err) : e(err), errorWhat(err.what()) {}
    void raise() const { throw *this; }
    ThreadDbException* clone() const { return new ThreadDbException(*this); }
    std::exception error() const { return e; }
    const char* what() const noexcept
    {
        return errorWhat.c_str();
    }
private:
    std::exception e;
    std::string errorWhat;


};
#endif // BOKZDBEXCEPTIONS_H



