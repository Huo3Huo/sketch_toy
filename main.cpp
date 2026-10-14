#define XXH_INLINE_ALL
#include "xxhash.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>
#include <vector>
#include <limits>
#include <cmath>
#include <stdexcept>

// Read the file
std::string readFile(const std::string& filename){
    std::ifstream file(filename);
    if (!file) {
        std::cerr <<"Failed to open " <<filename <<'\n';
        return "";
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Normalize words using a simple stemming function
std:: string stem(std::string word){
    // Convert to lowercase
    std::transform(word.begin(), word.end(), word.begin(), [](unsigned char c){return static_cast<char>(std::tolower(c));});

    //Remove everything except lowercase English letters a-z
    word.erase(
        std::remove_if(word.begin(), word.end(), [](unsigned char c){return c<'a' || c>'z';}),
        word.end()
    );

    //Remove one suffix
    auto endsWith = [&word](const std::string& suffix){
        return word.size() >= suffix.size() && word.compare(word.size()-suffix.size(), suffix.size(), suffix) == 0;
    };
    if (endsWith("ing") && word.length() > 4) {
        word.erase(word.length() - 3);
    }else if (endsWith("ed") && word.length() > 3){
        word.erase(word.length() - 2);
    }else if (endsWith("s") && word.length() > 3 && !endsWith("ss")){
        word.erase(word.length() - 1);
    }else if (endsWith("ly") && word.length() > 3){
        word.erase(word.length() - 2);
    }else if (endsWith("er") && word.length() > 4){
        word.erase(word.length() - 2);
    }else if (endsWith("est") && word.length() > 4){
        word.erase(word.length() - 3);
    }

    return word;
}

// Split text into words and normalize each word
std::vector<std::string> toWords(const std::string& text){
    std::vector<std::string> words;

    std::istringstream textStream(text);
    std::string line;

    while (std::getline(textStream, line)){
        std::istringstream lineStream(line);
        std::string word;

        while (std::getline(lineStream, word, ' ')){
            word = stem(word);
            // Ignore empty strings
            if (!word.empty()) {words.push_back(word);}
        }
    }

    return words;
}

// Get exact counts
std::map<std::string, int> countWords(const std::vector<std::string>& words){
    std::map<std::string, int> result;
    for (const std::string& word : words){
        result[word]++;
    }
    return result;
}

// Create a sketch
struct Sketch {
    std::size_t rows;
    std::size_t columns;
    std::vector<std::uint32_t> buckets;
};

Sketch createSketch(std::size_t rows, std::size_t columns){
    return {
        rows,
        columns,
        std::vector<std::uint32_t>(rows*columns, 0)
    };
}

Sketch sketchWithBounds (double errorRate, double confidence){
    if (errorRate <= 0.0 || errorRate > 1.0){
        throw std::invalid_argument("errorRate must be in (0, 1]");
    }
    if (confidence <= 0.0 || confidence >= 1.0){
        throw std::invalid_argument("confidence must be in (0, 1)");
    }

    const std::size_t columns = static_cast<std::size_t>(
        std::ceil(std::exp(1.0) / errorRate)
    );
    const std::size_t rows = static_cast<std::size_t>(
        std::ceil(std::log(1.0 / (1.0 - confidence)))
    );

    return createSketch(rows, columns);
}

// Add a word to sketch
void add(Sketch& sketch, const std::string word){
    for (std::size_t rowIdx = 0; rowIdx < sketch.rows; rowIdx++){
        // Hash the word with a different seed for each row
        const XXH64_hash_t hash = XXH3_64bits_withSeed(word.data(), word.size(), static_cast<XXH64_hash_t>(rowIdx));
        // Find the column within this row
        const std::size_t columnIdx = static_cast<std::size_t>(hash % sketch.columns);
        // Convert the row and column to a flat array index
        const std::size_t globalIndx = rowIdx * sketch.columns + columnIdx;
        // Increment the corresponding counter
        sketch.buckets[globalIndx]++;
    }
}

// check
std::uint32_t check(const Sketch& sketch, const std::string& word){
    std::uint32_t approx = std::numeric_limits<std::uint32_t>::max();
    for (std::size_t rowIdx = 0; rowIdx < sketch.rows; rowIdx++){
        const XXH64_hash_t hash = XXH3_64bits_withSeed(word.data(), word.size(), static_cast<XXH64_hash_t>(rowIdx));
        const std::size_t columnIdx = static_cast<std::size_t>(hash % sketch.columns);
        const std::size_t globalIndx = rowIdx * sketch.columns + columnIdx;
        if (sketch.buckets[globalIndx] < approx){
            approx = sketch.buckets[globalIndx];
        }
    }

    return approx;
}

int main(){
    const std::string wodehouse = readFile("wodehouse.txt");
    if (wodehouse.empty()){
        std::cerr << "The inpu file is empty or could not be read" << '\n';
        return 1;
    }

    const std::vector<std::string> words = toWords(wodehouse);
    const std::map<std::string, int> exactCounts = countWords(words);

    std::cout << "exactCounts {" << '\n';
    for (const auto& entry : exactCounts){
        std::cout << "  " << entry.first << ":" << entry.second << "\n";
    }
    std::cout << "}" << '\n';

    Sketch sketch = createSketch(2, 5);
    std::cout << "created:\n";
    std::cout << "rows: " << sketch.rows << '\n';
    std::cout << "columns: " << sketch.columns << '\n';
    std::cout << "buckets: ";
    for (std::uint32_t bucket: sketch.buckets){
        std::cout << bucket << ' ';
    }
    std::cout << '\n';
    
    add(sketch, stem("castle"));
    std::cout << "after adding 'castle' buckets: ";
    for (std::uint32_t bucket: sketch.buckets){
        std::cout << bucket << ' ';
    }
    std::cout << '\n';

    std::cout << "check castle: " << check(sketch, "castle") << '\n';

    // full text
    std::cout << "---Full Text---" << '\n';
    const std::string allWodehouse = readFile("wodehouse-full.txt");
    const std::vector<std::string> allWords = toWords(allWodehouse);
    const std::map<std::string, int> allExactCounts = countWords(allWords);
    const auto it = allExactCounts.find(stem("beetle"));
    if (it != allExactCounts.end()){
        std::cout << "exact 'beetle': " << it->second << '\n';
    }else{
        std::cout << "'beetle' not founde\n";
    }

    //Sketch allSketch = createSketch(5, 5437);
    Sketch allSketch = sketchWithBounds(0.0005, 0.99);
    std::cout << "columns: " << allSketch.columns << '\n';
    std::cout << "rows: " << allSketch.rows << '\n';
    for (const std::string word: allWords){
        add(allSketch, word);
    }
    std::cout << "check allSketch 'beetle': " << check(allSketch, stem("beetle")) << '\n';

    return 0;
}