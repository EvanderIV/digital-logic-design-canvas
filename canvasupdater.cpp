#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdlib> // Required for system()
#include <ctime>   // Required for date/time manipulation
#include <iomanip> // Required for std::get_time for parsing dates
#include <stdexcept>
#include <utility> // Required for std::pair

// Forward declarations for helper functions
bool parseStartDate(const std::string& dateStr, std::tm& startDate);
std::tm addDays(std::tm baseDate, int days);
std::string formatDate(const std::tm& date, std::string format);
void processFile(const std::filesystem::path& filePath, const std::tm& startDate, int startIndex);
void processDirectory(const std::filesystem::path& dirPath, const std::tm& startDate, int startIndex);
void rezipDirectory(const std::string& sourceDir, const std::filesystem::path& archivePath);

/**
 * @brief Main entry point of the program.
 * @param argc Argument count.
 * @param argv Argument values.
 * @return 0 on success, 1 on error.
 */
int main(int argc, char* argv[]) {
    // --- 1. Argument Parsing ---
    std::string startDateStr;
    std::string archivePathStr;
    std::string outputArchivePathStr;
    int startIndex = 0; // Default to 0-indexed

    // A more flexible argument parsing loop
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-start" && i + 1 < argc) {
            startDateStr = argv[++i]; // Consume next argument
        } else if (arg == "-o" && i + 1 < argc) {
            outputArchivePathStr = argv[++i]; // Consume next argument
        } else if (arg == "-i" && i + 1 < argc) {
            try {
                startIndex = std::stoi(argv[++i]); // Consume and parse index
            } catch (const std::exception& e) {
                std::cerr << "Error: Invalid number for -i argument." << std::endl;
                return 1;
            }
        } else {
            // Assume any other argument is the input file
            archivePathStr = arg;
        }
    }

    if (startDateStr.empty() || archivePathStr.empty()) {
        std::cerr << "Usage: " << argv[0] << " -start MM/DD/YYYY <input_archive.imscc> [-o <output_archive.imscc>] [-i <start_index>]" << std::endl;
        return 1;
    }

    // --- 1a. Generate default output path if not provided ---
    if (outputArchivePathStr.empty()) {
        std::filesystem::path inputPath(archivePathStr);
        std::string newFilename = inputPath.stem().string() + "_updated" + inputPath.extension().string();
        outputArchivePathStr = inputPath.replace_filename(newFilename).string();
    }


    std::filesystem::path archivePath(archivePathStr);
    std::tm startDate = {};

    if (!parseStartDate(startDateStr, startDate)) {
        std::cerr << "Error: Invalid start date format. Please use MM/DD/YYYY." << std::endl;
        return 1;
    }

    if (!std::filesystem::exists(archivePath)) {
        std::cerr << "Error: Archive file not found at '" << archivePath << "'" << std::endl;
        return 1;
    }

    // --- 2. Unzip the Archive ---
    std::string outputDir = "unzipped_archive";
    // Create the unzip command. -o overwrites files without prompting.
    std::string command = "unzip -o \"" + archivePath.string() + "\" -d \"" + outputDir + "\"";

    std::cout << "Unzipping archive..." << std::endl;
    int result = std::system(command.c_str());

    if (result != 0) {
        std::cerr << "Error: Failed to unzip the archive. Make sure the 'unzip' command is installed and in your system's PATH." << std::endl;
        return 1;
    }
    std::cout << "Archive successfully unzipped to '" << outputDir << "' directory." << std::endl;

    // --- 3. Process Files ---
    std::cout << "Processing files for date replacement..." << std::endl;
    try {
        processDirectory(outputDir, startDate, startIndex);
    } catch (const std::exception& e) {
        std::cerr << "An error occurred during file processing: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Date replacement complete." << std::endl;

    // --- 4. Re-zip the directory ---
    std::cout << "Re-zipping the archive..." << std::endl;
    rezipDirectory(outputDir, outputArchivePathStr);

    return 0;
}

/**
 * @brief Parses a date string in MM/DD/YYYY format into a std::tm struct.
 * @param dateStr The date string to parse.
 * @param startDate The std::tm struct to populate.
 * @return True if parsing was successful, false otherwise.
 */
bool parseStartDate(const std::string& dateStr, std::tm& startDate) {
    std::istringstream ss(dateStr);
    ss >> std::get_time(&startDate, "%m/%d/%Y");
    return !ss.fail();
}

/**
 * @brief Adds a specified number of days to a base date.
 * @param baseDate The starting date.
 * @param days The number of days to add.
 * @return A new std::tm struct representing the calculated date.
 */
std::tm addDays(std::tm baseDate, int days) {
    baseDate.tm_mday += days; // Add the days
    // mktime normalizes the date (e.g., handles month/year rollovers)
    // and also correctly sets tm_wday and tm_yday.
    std::mktime(&baseDate);
    return baseDate;
}

/**
 * @brief Formats a date according to a custom format string.
 * @param date The date to format.
 * @param format The format string (e.g., "M D, Y", "MM/DD/YYYY").
 * @return The formatted date string.
 */
std::string formatDate(const std::tm& date, std::string format) {
    // --- Create replacement values ---
    std::string yearStr = std::to_string(date.tm_year + 1900);
    
    const char* monthNames[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
    std::string monthFullNameStr = monthNames[date.tm_mon];
    const char* monthAbbrs[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    std::string monthAbbrStr = monthAbbrs[date.tm_mon];
    
    const char* dayNames[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    std::string dayFullNameStr = dayNames[date.tm_wday];
    const char* dayAbbrs[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    std::string dayNameAbbrStr = dayAbbrs[date.tm_wday];

    std::ostringstream dayStream;
    dayStream << std::setw(2) << std::setfill('0') << date.tm_mday;
    std::string dayPaddedStr = dayStream.str();
    std::string dayStr = std::to_string(date.tm_mday);

    // --- Create a list of replacements, LONGEST first to avoid substring conflicts ---
    std::vector<std::pair<std::string, std::string>> replacements = {
        {"YYYY", yearStr},
        {"MM", monthFullNameStr},
        {"NN", dayFullNameStr},
        {"DD", dayPaddedStr},
        {"Y", yearStr},
        {"M", monthAbbrStr},
        {"N", dayNameAbbrStr},
        {"D", dayStr}
    };
    
    // --- Apply replacements ---
    for (const auto& repl : replacements) {
        const std::string& from = repl.first;
        const std::string& to = repl.second;
        size_t start_pos = 0;
        while ((start_pos = format.find(from, start_pos)) != std::string::npos) {
            format.replace(start_pos, from.length(), to);
            start_pos += to.length(); // Advance past the replacement to avoid infinite loops
        }
    }
    
    return format;
}


/**
 * @brief Scans and processes a single file for DateReplace directives.
 * @param filePath The path to the file to process.
 * @param startDate The school year's start date.
 * @param startIndex The starting index for day numbers (e.g., 0 or 1).
 */
void processFile(const std::filesystem::path& filePath, const std::tm& startDate, int startIndex) {
    // Only process certain file types to avoid corrupting binary files
    const std::vector<std::string> validExtensions = {".html", ".htm", ".xml", ".txt"};
    std::string extension = filePath.extension().string();
    bool isValid = false;
    for(const auto& ext : validExtensions){
        if(extension == ext){
            isValid = true;
            break;
        }
    }
    if(!isValid) return;


    std::ifstream fileIn(filePath);
    if (!fileIn) {
        std::cerr << "Warning: Could not open file " << filePath << ". Skipping." << std::endl;
        return;
    }

    std::stringstream buffer;
    buffer << fileIn.rdbuf();
    std::string content = buffer.str();
    fileIn.close();

    bool modified = false;
    size_t searchPos = 0;
    const std::string startMarker = "DateReplace("; // FIX: Define the search marker

    while ((searchPos = content.find(startMarker, searchPos)) != std::string::npos) {
        modified = true;
        size_t openParenPos = searchPos + startMarker.length();
        size_t closeParenPos = content.find(")", openParenPos);
        if (closeParenPos == std::string::npos) continue; // Malformed, skip

        // --- FIX: Define the boundaries for the text to be replaced ---
        // It's located between the `>` after the directive and the very next `<`.
        size_t replaceStartPos = content.find(">", closeParenPos);
        if (replaceStartPos == std::string::npos) continue; // Malformed HTML, skip.
        replaceStartPos += 1; // The replacement starts *after* the '>'.

        size_t replaceEndPos = content.find("<", replaceStartPos);
        if (replaceEndPos == std::string::npos) continue; // Malformed HTML, skip.

        // --- 2. Parse the arguments from inside the parentheses ---
        std::string argsStr = content.substr(openParenPos, closeParenPos - openParenPos);
        size_t commaPos = argsStr.rfind(',');

        std::string formatStr;
        int dayOffset = 0;

        if (commaPos == std::string::npos) {
            // Case 1: No comma found. Treat the whole string as the format.
            // Default to the startIndex, which will result in a final offset of 0 days.
            formatStr = argsStr;
            dayOffset = startIndex;
        } else {
            // Case 2: Comma found. Parse as usual.
            formatStr = argsStr.substr(0, commaPos);
            std::string dayOffsetStr = argsStr.substr(commaPos + 1);

            // --- DEBUGGING OUTPUT ---
            std::cout << "[DEBUG] In file: " << filePath.filename().string() << "\n"
                      << "        Full directive args: \"" << argsStr << "\"\n"
                      << "        Attempting to parse day number from: \"" << dayOffsetStr << "\"" << std::endl;
            // --- END DEBUGGING OUTPUT ---
            
            try {
                dayOffset = std::stoi(dayOffsetStr);
            } catch (const std::exception& e) {
                std::cerr << "Warning: Invalid day number in \"" << filePath.string() << "\". Skipping this instance." << std::endl;
                searchPos = closeParenPos; // Advance search position to avoid infinite loop
                continue;
            }
        }

        // Trim quotes, underscores, parentheses, and whitespace
        formatStr.erase(0, formatStr.find_first_not_of(" \t\n\r\"_()"));
        formatStr.erase(formatStr.find_last_not_of(" \t\n\r\"_()") + 1);

        // --- 3. Calculate and format the new date ---
        // The day number from the file is adjusted by the start index to get the final offset.
        int finalDayOffset = dayOffset - startIndex;

        std::tm targetDate = addDays(startDate, finalDayOffset);
        std::string newDateStr = formatDate(targetDate, formatStr);

        // --- 4. Replace the content ---
        content.replace(replaceStartPos, replaceEndPos - replaceStartPos, newDateStr);

        // --- 5. Update search position to continue after the modified section ---
        searchPos = replaceStartPos + newDateStr.length();
    }

    if (modified) {
        std::ofstream fileOut(filePath, std::ios::trunc);
        if (!fileOut) {
            std::cerr << "Warning: Could not write to file " << filePath << ". Skipping." << std::endl;
            return;
        }
        fileOut << content;
        fileOut.close();
    }
}

/**
 * @brief Recursively iterates through a directory and processes each file.
 * @param dirPath The directory to process.
 * @param startDate The school year's start date.
 * @param startIndex The starting index for day numbers.
 */
void processDirectory(const std::filesystem::path& dirPath, const std::tm& startDate, int startIndex) {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dirPath)) {
        if (entry.is_regular_file()) {
            processFile(entry.path(), startDate, startIndex);
        }
    }
}

/**
 * @brief Zips the contents of a directory into a new archive file.
 * @param sourceDir The directory whose contents should be zipped.
 * @param archivePath The path for the output archive file.
 */
void rezipDirectory(const std::string& sourceDir, const std::filesystem::path& archivePath) {
    // To create a zip with the correct internal structure, we must run the zip
    // command from *inside* the source directory.
    // We use absolute paths to ensure correctness regardless of execution location.
    std::filesystem::path absoluteArchivePath = std::filesystem::absolute(archivePath);

    // The command changes into the source directory, then creates a zip file
    // containing all of its contents ('.'). The '-r' flag makes it recursive.
    std::string command = "cd " + sourceDir + " && zip -r \"" + absoluteArchivePath.string() + "\" .";

    int result = std::system(command.c_str());

    if (result != 0) {
        std::cerr << "Error: Failed to re-zip the directory. Make sure the 'zip' command is installed and in your system's PATH." << std::endl;
    } else {
        std::cout << "Successfully created new archive at '" << absoluteArchivePath.string() << "'" << std::endl;
    }
}










