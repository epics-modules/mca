#include <string>
#include <vector>

namespace stringSplit
{
    using namespace std;
    typedef string::size_type (string::*FindToken)(const string& strDelimiter, string::size_type sztOffset) const;
    // Split a string using a delimiter string or string character set, returns split string as a vector<string>
	// strIn - string being split
	// strDelimiters - delimiter string or character list in a string for splitting strIn
	// bRemoveEmpty - remove empty strings from list
	// bMatchAll - true=use delimiter string, false=use string as character set matching any chars in set
    vector<string> Split(const string& strIn, const string& strDelimiters, bool bRemoveEmpty=false, bool bMatchAll=false)
    {
        vector<string> VecStrTokens;						// return container for strTokens
        string::size_type SearchStart = 0;					// starting position for searches
        string::size_type SkipPosition = 1;					// positions to SkipPosition after a strDelimiters
        FindToken FindPosition = &string::find_first_of;	// search algorithm for matches
        if (bMatchAll)
        {
            SkipPosition = strDelimiters.length();
            FindPosition = &string::find;
        }
        while (SearchStart != string::npos)
        {     
            string::size_type SearchEnd = (strIn.*FindPosition)(strDelimiters, SearchStart);	// get a complete range [SearchStart..SearchEnd)
            if (SkipPosition == 0) SearchEnd = string::npos;	// no match ret whole string if skip is null
            string strToken = strIn.substr(SearchStart, SearchEnd - SearchStart);
            if (!(bRemoveEmpty && strToken.empty()))
            {
                VecStrTokens.push_back(strToken);				  // extract the strToken and add it to the VecStrTokens list
            }
            if ((SearchStart = SearchEnd) != string::npos) SearchStart += SkipPosition; // SearchStart the next range
        }
        return VecStrTokens;
    }
}
