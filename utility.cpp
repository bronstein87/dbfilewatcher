#include "utility.h"

namespace Utility {
QString translate (const QString& from, bool inverted)
{
    const QStringList rus = {"А", "а", "Б", "б", "В", "в", "Г", "г", "Д", "д", "Е", "е", "Ж",
                             "ж", "З", "з", "И", "и", "Й", "й", "К", "к",
                             "Л", "л", "М", "м", "Н", "н", "О", "о", "П", "п", "Р", "р", "С",
                             "с", "Т", "т", "У", "у", "Ф", "ф", "Х", "х",
                             "Ц", "ц", "Ч", "ч", "Ш", "ш","Щ", "щ", "Ь", "ь", "Ю", "ю", "Я",
                             "я", "Ы", "ы", "Ъ", "ъ", "Ё", "ё", "Э", "э"};

    const QStringList eng = {"A", "a", "B", "b", "V", "v", "G", "g", "D", "d", "E", "e","Zh",
                             "zh", "Z", "z", "I", "i", "J", "j", "K", "k",
                             "L", "l", "M", "m", "N", "n", "O", "o", "P", "p",  "R", "r", "S",
                             "s", "T", "t", "U", "u", "F", "f","H", "h", "Ts", "ts",
                             "ch", "ch", "Sh", "sh", "Shh", "shh", "i", "i", "Yu", "yu","Ya",
                             "ya", "Y", "y", "", "", "Yo", "yo", "E", "e"};

    const QStringList* fromAlphabet = &rus;
    const QStringList* toAlphabet = &eng;
    if (inverted)
    {
        toAlphabet = &rus;
        fromAlphabet = &eng;
    }
    int pos = -1;
    QString result;
    for (const auto& i : from)
    {
        if ((pos = fromAlphabet->indexOf(i)) != -1)
        {
            result.append(toAlphabet->at(pos));
        }
        else
        {
            result.append(i);
        }
    }
    return result;

}
}
