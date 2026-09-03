#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef _WINBASE_
extern "C" __declspec(dllimport) unsigned long __stdcall GetLogicalDrives(void);
extern "C" __declspec(dllimport) unsigned int __stdcall GetDriveTypeA(const char *);
#endif
#endif

#ifndef IMGUI_VERSION
#   error "include imgui.h before this header"
#endif

using ImGuiFileBrowserFlags = std::uint32_t;

enum ImGuiFileBrowserFlags_ : std::uint32_t
{
    ImGuiFileBrowserFlags_SelectDirectory       = 1 << 0,  // select directory instead of regular file
    ImGuiFileBrowserFlags_EnterNewFilename      = 1 << 1,  // allow user to enter new filename when selecting regular file
    ImGuiFileBrowserFlags_NoModal               = 1 << 2,  // file browsing window is modal by default. specify this to use a popup window
    ImGuiFileBrowserFlags_NoTitleBar            = 1 << 3,  // hide window title bar
    ImGuiFileBrowserFlags_NoStatusBar           = 1 << 4,  // hide status bar at the bottom of browsing window
    ImGuiFileBrowserFlags_CloseOnEsc            = 1 << 5,  // close file browser when pressing 'ESC'
    ImGuiFileBrowserFlags_CreateNewDir          = 1 << 6,  // allow user to create new directory
    ImGuiFileBrowserFlags_MultipleSelection     = 1 << 7,  // allow user to select multiple files. this will hide ImGuiFileBrowserFlags_EnterNewFilename
    ImGuiFileBrowserFlags_HideRegularFiles      = 1 << 8,  // hide regular files when ImGuiFileBrowserFlags_SelectDirectory is enabled
    ImGuiFileBrowserFlags_ConfirmOnEnter        = 1 << 9,  // confirm selection when pressing 'ENTER'
    ImGuiFileBrowserFlags_SkipItemsCausingError = 1 << 10, // when entering a new directory, any error will interrupt the process, causing the file browser to fall back to the working directory.
                                                           // with this flag, if an error is caused by a specific item in the directory, that item will be skipped, allowing the process to continue.
    ImGuiFileBrowserFlags_EditPathString        = 1 << 11, // allow user to directly edit the whole path string
};

namespace ImGui
{
    class FileBrowser
    {
    public:

        explicit FileBrowser(
            ImGuiFileBrowserFlags flags = 0,
            std::filesystem::path defaultDirectory = std::filesystem::current_path());

        FileBrowser(const FileBrowser &copyFrom);

        FileBrowser &operator=(const FileBrowser &copyFrom);

        ~FileBrowser();

        // set the window position (in pixels)
        // default is centered
        void SetWindowPos(int posX, int posY) noexcept;

        // set the window size (in pixels)
        // default is (700, 450)
        void SetWindowSize(int width, int height) noexcept;

        // set the window title text
        void SetTitle(std::string title);

        // open the browsing window
        void Open();

        // close the browsing window
        void Close();

        // the browsing window is opened or not
        bool IsOpened() const noexcept;

        // display the browsing window if opened
        void Display();

        // returns true when there is a selected filename
        bool HasSelected() const noexcept;

        // set current browsing directory
        bool SetDirectory(const std::filesystem::path &dir = std::filesystem::current_path());

        // legacy interface. use SetDirectory instead.
        bool SetPwd(const std::filesystem::path &dir = std::filesystem::current_path())
        {
            return SetDirectory(dir);
        }

        // get current browsing directory
        const std::filesystem::path &GetDirectory() const noexcept;

        // legacy interface. use GetDirectory instead.
        const std::filesystem::path &GetPwd() const noexcept
        {
            return GetDirectory();
        }

        // returns selected filename. make sense only when HasSelected returns true
        // when ImGuiFileBrowserFlags_MultipleSelection is enabled, only one of
        // selected filename will be returned
        std::filesystem::path GetSelected() const;

        // returns all selected filenames.
        // when ImGuiFileBrowserFlags_MultipleSelection is enabled, use this
        // instead of GetSelected
        std::vector<std::filesystem::path> GetMultiSelected() const;

        // set selected filename to empty
        void ClearSelected();

        // (optional) set file type filters. eg. { ".h", ".cpp", ".hpp" }
        // ".*" matches any file types
        void SetTypeFilters(const std::vector<std::string> &typeFilters);

        // set currently applied type filter
        // default value is 0 (the first type filter)
        void SetCurrentTypeFilterIndex(int index);

        // current type-filter string (e.g. ".gcode"); empty when none set
        const std::string &GetCurrentTypeFilter() const noexcept;

        // when ImGuiFileBrowserFlags_EnterNewFilename is set
        // this function will pre-fill the input dialog with a filename.
        void SetInputName(std::string_view input);

        // Left quick-access sidebar (RigKit). Empty = no sidebar.
        void ClearQuickAccess();
        void AddQuickAccess(std::string label, std::filesystem::path path);
        void SetQuickAccessWidth(float width) noexcept;

    private:
        struct QuickAccessEntry
        {
            std::string           label;
            std::filesystem::path path;
        };

        template <class Functor>
        struct ScopeGuard
        {
            ScopeGuard(Functor&& t) : func(std::move(t)) { }

            ~ScopeGuard() { func(); }

        private:

            Functor func;
        };

        struct FileRecord
        {
            bool                  isDir = false;
            std::filesystem::path name;
            std::string           showName;
            std::filesystem::path extension;
            std::uint64_t         size = 0;
            std::int64_t          lastWriteUnix = 0;
            std::string           sizeText;
            std::string           typeText;
            std::string           dateText;
        };

        static std::string ToLower(const std::string &s);

        void ToolTip(const std::string_view &s);

        void UpdateFileRecords();

        static std::vector<FileRecord> CollectFileRecords(
            const std::filesystem::path &dir, ImGuiFileBrowserFlags flags);

        void RequestListing();
        void ReapListing();
        void PumpListing();
        void ShowListingPlaceholder();

        static std::string FormatByteSize(std::uint64_t bytes);

        static std::string FormatFileTime(const std::filesystem::file_time_type &tp);

        static int CompareRecordNames(const FileRecord &a, const FileRecord &b);

        void SortFileRecords(const ImGuiTableSortSpecs *sortSpecs);

        void SetCurrentDirectoryUncatched(const std::filesystem::path &pwd);

        bool SetCurrentDirectoryInternal(
            const std::filesystem::path &dir,
            const std::filesystem::path &preferredFallback);

        bool IsExtensionMatched(const std::filesystem::path &extension) const;

        void ClearRangeSelectionState();

        static void AssignToArrayStyleString(std::vector<char> &arr, std::string_view content);

        static int ExpandInputBuffer(ImGuiInputTextCallbackData *callbackData);

#ifdef _WIN32
        static std::uint32_t GetDrivesBitMask();
        void RequestDriveScan(bool force);
        void ReapDriveScan();
#endif

        // for c++17 compatibility

#if defined(__cpp_lib_char8_t)
        static std::string u8StrToStr(std::u8string s);
#endif
        static std::string u8StrToStr(std::string s);

        static std::filesystem::path u8StrToPath(const char *str);

        int width_;
        int height_;
        int posX_;
        int posY_;
        ImGuiFileBrowserFlags flags_;
        std::filesystem::path defaultDirectory_;

        std::string title_;
        std::string openLabel_;

        bool shouldOpen_;
        bool shouldClose_;
        bool isOpened_;
        bool isOk_;
        bool isPosSet_;

        std::string statusStr_;

        std::vector<std::string> typeFilters_;
        unsigned int             typeFilterIndex_;
        bool                     hasAllFilter_;

        std::filesystem::path   currentDirectory_;
        std::vector<FileRecord> fileRecords_;
        bool                    needSort_ = true;

        struct ListingResult
        {
            std::uint64_t             generation = 0;
            std::filesystem::path     directory;
            std::vector<FileRecord>   records;
            std::string               error;
        };
        std::filesystem::path     listingPath_;
        std::filesystem::path     listingFallback_;
        std::atomic<std::uint64_t> listingGen_{0};
        std::atomic<bool>         listingBusy_{false};
        std::atomic<bool>         listingStale_{false};
        std::thread               listingThread_;
        std::mutex                listingMutex_;
        ListingResult             listingPending_;

        unsigned int                    rangeSelectionStart_; // enable range selection when shift is pressed
        std::set<std::filesystem::path> selectedFilenames_;

        std::string       openNewDirLabel_;
        std::vector<char> newDirNameBuffer_;
        std::vector<char> inputNameBuffer_;
        std::string       customizedInputName_;

        bool              editDir_;
        bool              setFocusToEditDir_;
        std::vector<char> currDirBuffer_;

#ifdef _WIN32
        std::uint32_t                 drives_ = 0;
        std::atomic<std::uint32_t>    drivesLive_{0};
        std::atomic<unsigned int>     driveType_[26];
        std::atomic<bool>             driveScanBusy_{false};
        std::thread                   driveScanThread_;
        double                        lastDriveScanTime_ = -1.0;
#endif

        std::vector<QuickAccessEntry> quickAccess_;
        float                         quickAccessWidth_ = 140.f;
    };
} // namespace ImGui

inline ImGui::FileBrowser::FileBrowser(ImGuiFileBrowserFlags flags, std::filesystem::path defaultDirectory)
    : width_(700)
    , height_(450)
    , posX_(0)
    , posY_(0)
    , flags_(flags)
    , defaultDirectory_(std::move(defaultDirectory))
    , shouldOpen_(false)
    , shouldClose_(false)
    , isOpened_(false)
    , isOk_(false)
    , isPosSet_(false)
    , rangeSelectionStart_(0)
    , editDir_(false)
    , setFocusToEditDir_(false)
{
    assert(!((flags_ & ImGuiFileBrowserFlags_SelectDirectory) && (flags_ & ImGuiFileBrowserFlags_EnterNewFilename)) &&
           "'EnterNewFilename' doesn't work when 'SelectDirectory' is enabled");
    if(flags_ & ImGuiFileBrowserFlags_CreateNewDir)
    {
        newDirNameBuffer_.resize(32, '\0');
    }

    SetTitle("file browser");
    SetDirectory(defaultDirectory_);

    typeFilters_.clear();
    typeFilterIndex_ = 0;
    hasAllFilter_ = false;

#ifdef _WIN32
    const std::uint32_t mask = GetDrivesBitMask();
    drives_ = mask;
    drivesLive_.store(mask, std::memory_order_relaxed);
#endif
}

inline ImGui::FileBrowser::FileBrowser(const FileBrowser &copyFrom)
    : FileBrowser()
{
    *this = copyFrom;
}

inline ImGui::FileBrowser &ImGui::FileBrowser::operator=(
    const FileBrowser &copyFrom)
{
    if(listingThread_.joinable())
    {
        listingThread_.join();
    }
#ifdef _WIN32
    ReapDriveScan();
    if(driveScanThread_.joinable())
    {
        driveScanThread_.join();
    }
#endif

    width_  = copyFrom.width_;
    height_ = copyFrom.height_;

    posX_ = copyFrom.posX_;
    posY_ = copyFrom.posY_;

    flags_ = copyFrom.flags_;
    SetTitle(copyFrom.title_);

    shouldOpen_  = copyFrom.shouldOpen_;
    shouldClose_ = copyFrom.shouldClose_;
    isOpened_    = copyFrom.isOpened_;
    isOk_        = copyFrom.isOk_;
    isPosSet_    = copyFrom.isPosSet_;

    statusStr_ = "";

    typeFilters_     = copyFrom.typeFilters_;
    typeFilterIndex_ = copyFrom.typeFilterIndex_;
    hasAllFilter_    = copyFrom.hasAllFilter_;

    selectedFilenames_   = copyFrom.selectedFilenames_;
    rangeSelectionStart_ = copyFrom.rangeSelectionStart_;

    currentDirectory_ = copyFrom.currentDirectory_;
    fileRecords_      = copyFrom.fileRecords_;
    needSort_         = true;

    openNewDirLabel_     = copyFrom.openNewDirLabel_;
    newDirNameBuffer_    = copyFrom.newDirNameBuffer_;
    inputNameBuffer_     = copyFrom.inputNameBuffer_;
    customizedInputName_ = copyFrom.customizedInputName_;

    editDir_ = copyFrom.editDir_;
    currDirBuffer_ = copyFrom.currDirBuffer_;

#ifdef _WIN32
    ReapDriveScan();
    drives_ = copyFrom.drivesLive_.load(std::memory_order_relaxed);
    drivesLive_.store(drives_, std::memory_order_relaxed);
    for(int i = 0; i < 26; ++i)
    {
        driveType_[i].store(copyFrom.driveType_[i].load(std::memory_order_relaxed),
                            std::memory_order_relaxed);
    }
    lastDriveScanTime_ = copyFrom.lastDriveScanTime_;
#endif

    quickAccess_      = copyFrom.quickAccess_;
    quickAccessWidth_ = copyFrom.quickAccessWidth_;

    return *this;
}

inline void ImGui::FileBrowser::SetWindowPos(int posX, int posY) noexcept
{
    posX_ = posX;
    posY_ = posY;
    isPosSet_ = true;
}

inline void ImGui::FileBrowser::SetWindowSize(int width, int height) noexcept
{
    assert(width > 0 && height > 0);
    width_  = width;
    height_ = height;
}

inline void ImGui::FileBrowser::SetTitle(std::string title)
{
    title_ = std::move(title);

    const std::string thisPtrStr = std::to_string(reinterpret_cast<size_t>(this));
    openLabel_ = title_ + "##filebrowser_" + thisPtrStr;
    openNewDirLabel_ = "new dir##new_dir_" + thisPtrStr;
}

inline void ImGui::FileBrowser::Open()
{
#ifdef _WIN32
    RequestDriveScan(true);
#endif
    listingPath_ = currentDirectory_;
    RequestListing();
    ClearSelected();
    statusStr_ = std::string();
    shouldOpen_ = true;
    shouldClose_ = false;
    if((flags_ & ImGuiFileBrowserFlags_EnterNewFilename) && !customizedInputName_.empty())
    {
        AssignToArrayStyleString(inputNameBuffer_, customizedInputName_);
        selectedFilenames_ = { u8StrToPath(inputNameBuffer_.data()) };
    }
}

inline void ImGui::FileBrowser::Close()
{
    ClearSelected();
    statusStr_ = std::string();
    shouldClose_ = true;
    shouldOpen_ = false;
}

inline bool ImGui::FileBrowser::IsOpened() const noexcept
{
    return isOpened_;
}

inline void ImGui::FileBrowser::Display()
{
    PushID(this);
    ScopeGuard exitThis([this]
    {
        shouldOpen_ = false;
        shouldClose_ = false;
        PopID();
    });

    PumpListing();

    if(shouldOpen_)
    {
        OpenPopup(openLabel_.c_str());
    }
    isOpened_ = false;

    // open the popup window

    if(shouldOpen_ && (flags_ & ImGuiFileBrowserFlags_NoModal))
    {
        if(isPosSet_)
        {
            SetNextWindowPos(ImVec2(static_cast<float>(posX_), static_cast<float>(posY_)));
        }
        else if(const ImGuiViewport* vp = GetMainViewport())
        {
            SetNextWindowPos(vp->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        }
        SetNextWindowSize(ImVec2(static_cast<float>(width_), static_cast<float>(height_)));
    }
    else
    {
        // Modal: center on the work viewport unless the host set an explicit
        // corner. Size uses Appearing so hosts can bump DPI-scaled layout on
        // every Open() (FirstUseEver left undersized crumbs stuck in imgui.ini).
        if(isPosSet_)
        {
            SetNextWindowPos(ImVec2(static_cast<float>(posX_), static_cast<float>(posY_)), ImGuiCond_Appearing);
        }
        else if(const ImGuiViewport* vp = GetMainViewport())
        {
            SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        }
        SetNextWindowSize(ImVec2(static_cast<float>(width_), static_cast<float>(height_)), ImGuiCond_Appearing);
    }
    if(flags_ & ImGuiFileBrowserFlags_NoModal)
    {
        if(!BeginPopup(openLabel_.c_str()))
        {
            return;
        }
    }
    else if(!BeginPopupModal(openLabel_.c_str(), nullptr,
                             flags_ & ImGuiFileBrowserFlags_NoTitleBar ? ImGuiWindowFlags_NoTitleBar : 0))
    {
        return;
    }

    isOpened_ = true;
    ScopeGuard endPopup([] { EndPopup(); });

#ifdef _WIN32
    ReapDriveScan();
    drives_ = drivesLive_.load(std::memory_order_acquire);
    RequestDriveScan(false);
#endif

    std::filesystem::path newDir; bool shouldSetNewDir = false;

    if(editDir_)
    {
        if(setFocusToEditDir_) // Automatically set the text box to be focused on appearing
        {
            SetKeyboardFocusHere();
        }

        PushItemWidth(-1);
        const bool enter = InputText(
            "##directory", currDirBuffer_.data(), currDirBuffer_.size(),
            ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll,
            ExpandInputBuffer, &currDirBuffer_);
        PopItemWidth();

        if(!IsItemActive() && !setFocusToEditDir_)
        {
            editDir_ = false;
        }
        setFocusToEditDir_ = false;

        if(enter)
        {
            std::filesystem::path enteredDir = u8StrToPath(currDirBuffer_.data());
            newDir = std::move(enteredDir);
            shouldSetNewDir = true;
        }
    }
    else
    {
        // display elements in pwd

#ifdef _WIN32
        const char currentDrive = static_cast<char>(currentDirectory_.c_str()[0]);
        const char driveStr[] = { currentDrive, ':', '\0' };

        PushItemWidth(4 * GetFontSize());
        if(BeginCombo("##select_drive", driveStr))
        {
            ScopeGuard guard([&] { EndCombo(); });

            for(int i = 0; i < 26; ++i)
            {
                if(!(drives_ & (1 << i)))
                {
                    continue;
                }

                const char driveCh = static_cast<char>('A' + i);
                const char selectableStr[] = { driveCh, ':', '\0' };
                const bool selected = currentDrive == driveCh;

                if(Selectable(selectableStr, selected) && !selected)
                {
                    char newPwd[] = { driveCh, ':', '\\', '\0' };
                    SetDirectory(newPwd);
                }
            }
        }
        PopItemWidth();

        SameLine();
#endif

        int secIdx = 0, newDirLastSecIdx = -1;
        for(const auto &sec : currentDirectory_)
        {
#ifdef _WIN32
            if(secIdx == 1)
            {
                ++secIdx;
                continue;
            }
#endif

            PushID(secIdx);
            if(secIdx > 0)
            {
                SameLine();
            }
            if(SmallButton(u8StrToStr(sec.u8string()).c_str()))
            {
                newDirLastSecIdx = secIdx;
            }
            PopID();

            ++secIdx;
        }

        if(newDirLastSecIdx >= 0)
        {
            int i = 0;
            std::filesystem::path dstDir;
            for(const auto &sec : currentDirectory_)
            {
                if(i++ > newDirLastSecIdx)
                {
                    break;
                }
                dstDir /= sec;
            }

#ifdef _WIN32
            if(newDirLastSecIdx == 0)
            {
                dstDir /= "\\";
            }
#endif

            SetDirectory(dstDir);
        }

        if(flags_ & ImGuiFileBrowserFlags_EditPathString)
        {
            SameLine();

            if(SmallButton("#"))
            {
                const auto currDirStr = u8StrToStr(currentDirectory_.u8string());
                currDirBuffer_.resize(currDirStr.size() + 1);
                std::memcpy(currDirBuffer_.data(), currDirStr.data(), currDirStr.size());
                currDirBuffer_.back() = '\0';

                editDir_ = true;
                setFocusToEditDir_ = true;
            }
            else
            {
                ToolTip("Edit the current path");
            }
        }
    }

    SameLine();
    if(SmallButton("Refresh"))
    {
#ifdef _WIN32
        RequestDriveScan(true);
#endif

        listingPath_ = currentDirectory_;
        RequestListing();

        std::set<std::filesystem::path> newSelectedFilenames;
        for(auto &name : selectedFilenames_)
        {
            const auto it = std::find_if(
                fileRecords_.begin(), fileRecords_.end(), [&](const FileRecord &record)
                {
                    return name == record.name;
                });
            if(it != fileRecords_.end())
            {
                newSelectedFilenames.insert(name);
            }
        }

        if((flags_ & ImGuiFileBrowserFlags_EnterNewFilename) && !inputNameBuffer_.empty() && inputNameBuffer_[0])
        {
            newSelectedFilenames.insert(u8StrToPath(inputNameBuffer_.data()));
        }
    }
    else
    {
        ToolTip("Refresh");
    }

    bool focusOnInputText = false;
    if(flags_ & ImGuiFileBrowserFlags_CreateNewDir)
    {
        SameLine();
        if(SmallButton("+"))
        {
            OpenPopup(openNewDirLabel_.c_str());
            newDirNameBuffer_[0] = '\0';
        }
        else
        {
            ToolTip("Create a new directory");
        }

        if(BeginPopup(openNewDirLabel_.c_str()))
        {
            ScopeGuard endNewDirPopup([] { EndPopup(); });

            InputText(
                "name", newDirNameBuffer_.data(), newDirNameBuffer_.size(),
                ImGuiInputTextFlags_CallbackResize, ExpandInputBuffer, &newDirNameBuffer_);
            focusOnInputText |= IsItemFocused();
            SameLine();

            if(Button("ok") && newDirNameBuffer_[0] != '\0')
            {
                ScopeGuard closeNewDirPopup([] { CloseCurrentPopup(); });
                if(create_directory(currentDirectory_ / u8StrToPath(newDirNameBuffer_.data())))
                {
                    listingPath_ = currentDirectory_;
                    RequestListing();
                }
                else
                {
                    statusStr_ = "failed to create " + std::string(newDirNameBuffer_.data());
                }
            }
        }
    }

    // browse files in a child window

    float reserveHeight = GetFrameHeightWithSpacing();
    if(flags_ & ImGuiFileBrowserFlags_EnterNewFilename)
    {
        reserveHeight += GetFrameHeightWithSpacing();
    }

    {
        const bool showQuick = !quickAccess_.empty();
        if(showQuick)
        {
            BeginChild("##quick_access", ImVec2(quickAccessWidth_, -reserveHeight), true);
            TextDisabled("Quick access");
            Separator();
            for(int i = 0; i < static_cast<int>(quickAccess_.size()); ++i)
            {
                const auto &entry = quickAccess_[static_cast<size_t>(i)];
                PushID(i);
                const bool selected = currentDirectory_ == entry.path;
                if(Selectable(entry.label.c_str(), selected))
                {
                    SetDirectory(entry.path);
                }
                {
                    const std::string tip = u8StrToStr(entry.path.u8string());
                    ToolTip(tip);
                }
                PopID();
            }
#ifdef _WIN32
            Separator();
            TextDisabled("Drives");
            for(int i = 0; i < 26; ++i)
            {
                if(!(drives_ & (1 << i)))
                {
                    continue;
                }
                const char driveCh = static_cast<char>('A' + i);
                const char selectableStr[] = { driveCh, ':', '\0' };
                char drivePwd[] = { driveCh, ':', '\\', '\0' };
                const bool selected =
                    !currentDirectory_.empty() &&
                    static_cast<char>(currentDirectory_.c_str()[0]) == driveCh;
                const unsigned int dtype =
                    driveType_[i].load(std::memory_order_relaxed);
                char rowLabel[32];
                if(dtype == 2) // DRIVE_REMOVABLE
                {
                    std::snprintf(rowLabel, sizeof(rowLabel), "%s  Removable", selectableStr);
                }
                else if(dtype == 4) // DRIVE_REMOTE
                {
                    std::snprintf(rowLabel, sizeof(rowLabel), "%s  Network", selectableStr);
                }
                else if(dtype == 5) // DRIVE_CDROM
                {
                    std::snprintf(rowLabel, sizeof(rowLabel), "%s  CD", selectableStr);
                }
                else
                {
                    std::snprintf(rowLabel, sizeof(rowLabel), "%s", selectableStr);
                }
                PushID(100 + i);
                if(Selectable(rowLabel, selected))
                {
                    SetDirectory(drivePwd);
                }
                PopID();
            }
#endif
            EndChild();
            SameLine(0.0f, 0.0f);

            // Draggable splitter between quick access and the file list.
            InvisibleButton("##quick_access_splitter", ImVec2(6.0f, -reserveHeight));
            if(IsItemHovered() || IsItemActive())
            {
                SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            }
            if(IsItemActive())
            {
                quickAccessWidth_ += GetIO().MouseDelta.x;
                const float maxWidth = GetWindowWidth() * 0.6f;
                if(quickAccessWidth_ < 48.0f)
                {
                    quickAccessWidth_ = 48.0f;
                }
                else if(quickAccessWidth_ > maxWidth)
                {
                    quickAccessWidth_ = maxWidth;
                }
            }
            {
                const ImVec2 mn = GetItemRectMin();
                const ImVec2 mx = GetItemRectMax();
                const float x = (mn.x + mx.x) * 0.5f;
                const ImU32 col = GetColorU32(
                    IsItemActive() ? ImGuiCol_SeparatorActive
                                   : (IsItemHovered() ? ImGuiCol_SeparatorHovered
                                                      : ImGuiCol_Separator));
                GetWindowDrawList()->AddLine(ImVec2(x, mn.y), ImVec2(x, mx.y), col, 1.0f);
            }
            SameLine(0.0f, 0.0f);
        }

        BeginChild("ch", ImVec2(0, -reserveHeight), true);
        ScopeGuard endChild([] { EndChild(); });

        const bool shouldHideRegularFiles =
            (flags_ & ImGuiFileBrowserFlags_HideRegularFiles) && (flags_ & ImGuiFileBrowserFlags_SelectDirectory);

        const ImGuiTableFlags tableFlags =
            ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable |
            ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
            ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV;
        if(BeginTable("##files", 4, tableFlags, ImVec2(0.0f, 0.0f)))
        {
            ScopeGuard endTable([] { EndTable(); });
            TableSetupScrollFreeze(0, 1);
            TableSetupColumn("Name",
                             ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort |
                                 ImGuiTableColumnFlags_NoHide,
                             0.0f, 0);
            TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending,
                             GetFontSize() * 5.5f, 1);
            TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, GetFontSize() * 7.0f, 2);
            TableSetupColumn("Date modified",
                             ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending,
                             GetFontSize() * 10.5f, 3);
            TableHeadersRow();

            if(ImGuiTableSortSpecs *sortSpecs = TableGetSortSpecs())
            {
                if(sortSpecs->SpecsDirty || needSort_)
                {
                    SortFileRecords(sortSpecs);
                    sortSpecs->SpecsDirty = false;
                    needSort_ = false;
                }
            }
            else if(needSort_)
            {
                SortFileRecords(nullptr);
                needSort_ = false;
            }

            for(unsigned int rscIndex = 0; rscIndex < fileRecords_.size(); ++rscIndex)
            {
                const auto &rsc = fileRecords_[rscIndex];
                if(!rsc.isDir && shouldHideRegularFiles)
                {
                    continue;
                }
                if(!rsc.isDir && !IsExtensionMatched(rsc.extension))
                {
                    continue;
                }
                if(!rsc.name.empty() && rsc.name.c_str()[0] == '$')
                {
                    continue;
                }

                const bool selected = selectedFilenames_.find(rsc.name) != selectedFilenames_.end();

#if IMGUI_VERSION_NUM >= 19100
                const ImGuiSelectableFlags selectableFlag =
                    ImGuiSelectableFlags_NoAutoClosePopups | ImGuiSelectableFlags_SpanAllColumns;
#else
                const ImGuiSelectableFlags selectableFlag =
                    ImGuiSelectableFlags_DontClosePopups | ImGuiSelectableFlags_SpanAllColumns;
#endif

                TableNextRow();
                TableSetColumnIndex(0);
                if(Selectable(rsc.showName.c_str(), selected, selectableFlag))
                {
                    const bool wantDir = flags_ & ImGuiFileBrowserFlags_SelectDirectory;
                    const bool canSelect = rsc.name != ".." && rsc.isDir == wantDir;
                    const bool rangeSelect =
                        canSelect && GetIO().KeyShift &&
                        rangeSelectionStart_ < fileRecords_.size() &&
                        (flags_ & ImGuiFileBrowserFlags_MultipleSelection) &&
                        IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
                    const bool multiSelect =
                        !rangeSelect && GetIO().KeyCtrl &&
                        (flags_ & ImGuiFileBrowserFlags_MultipleSelection) &&
                        IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

                    if(rangeSelect)
                    {
                        const unsigned int first = (std::min)(rangeSelectionStart_, rscIndex);
                        const unsigned int last = (std::max)(rangeSelectionStart_, rscIndex);
                        selectedFilenames_.clear();
                        for(unsigned int i = first; i <= last; ++i)
                        {
                            if(fileRecords_[i].isDir != wantDir)
                            {
                                continue;
                            }
                            if(!wantDir && !IsExtensionMatched(fileRecords_[i].extension))
                            {
                                continue;
                            }
                            selectedFilenames_.insert(fileRecords_[i].name);
                        }
                    }
                    else if(selected)
                    {
                        if(!multiSelect)
                        {
                            selectedFilenames_ = { rsc.name };
                            rangeSelectionStart_ = rscIndex;
                        }
                        else
                        {
                            selectedFilenames_.erase(rsc.name);
                        }
                        if(flags_ & ImGuiFileBrowserFlags_EnterNewFilename)
                        {
                            AssignToArrayStyleString(inputNameBuffer_, "");
                        }
                    }
                    else if(canSelect)
                    {
                        if(multiSelect)
                        {
                            selectedFilenames_.insert(rsc.name);
                        }
                        else
                        {
                            selectedFilenames_ = { rsc.name };
                        }
                        if(flags_ & ImGuiFileBrowserFlags_EnterNewFilename)
                        {
                            const auto rscName = u8StrToStr(rsc.name.u8string());
                            AssignToArrayStyleString(inputNameBuffer_, rscName);
                        }
                        rangeSelectionStart_ = rscIndex;
                    }
                }

                if(IsMouseDoubleClicked(ImGuiMouseButton_Left) && IsItemHovered(ImGuiHoveredFlags_None))
                {
                    if(rsc.isDir)
                    {
                        shouldSetNewDir = true;
                        newDir = (rsc.name != "..") ? (currentDirectory_ / rsc.name) : currentDirectory_.parent_path();
                    }
                    else if(!(flags_ & ImGuiFileBrowserFlags_SelectDirectory))
                    {
                        selectedFilenames_ = { rsc.name };
                        isOk_ = true;
                        CloseCurrentPopup();
                    }
                }
                else if(IsKeyPressed(ImGuiKey_GamepadFaceDown) && IsItemHovered())
                {
                    if(rsc.isDir)
                    {
                        shouldSetNewDir = true;
                        newDir = (rsc.name != "..") ? (currentDirectory_ / rsc.name) : currentDirectory_.parent_path();
                        SetKeyboardFocusHere(-1);
                    }
                    else if(!(flags_ & ImGuiFileBrowserFlags_SelectDirectory))
                    {
                        selectedFilenames_ = { rsc.name };
                        isOk_ = true;
                        CloseCurrentPopup();
                    }
                }

                TableSetColumnIndex(1);
                if(!rsc.isDir && !rsc.sizeText.empty())
                {
                    const float avail = GetContentRegionAvail().x;
                    const float tw = CalcTextSize(rsc.sizeText.c_str()).x;
                    if(tw < avail)
                    {
                        SetCursorPosX(GetCursorPosX() + avail - tw);
                    }
                    TextUnformatted(rsc.sizeText.c_str());
                }
                TableSetColumnIndex(2);
                TextUnformatted(rsc.typeText.c_str());
                TableSetColumnIndex(3);
                TextUnformatted(rsc.dateText.c_str());
            }
        }
    }

    if(shouldSetNewDir)
    {
        SetDirectory(newDir);
    }

    if(flags_ & ImGuiFileBrowserFlags_EnterNewFilename)
    {
        PushID(this);
        ScopeGuard popTextID([] { PopID(); });

        if(inputNameBuffer_.empty())
        {
            inputNameBuffer_.resize(1, '\0');
        }

        AlignTextToFramePadding();
        TextUnformatted("File name");
        SameLine();

        const bool showFilterHere = !typeFilters_.empty();
        const float filterW = showFilterHere ? 8.0f * GetFontSize() : 0.0f;
        const float gap = showFilterHere ? GetStyle().ItemSpacing.x : 0.0f;
        PushItemWidth(GetContentRegionAvail().x - filterW - gap);
        if(InputTextWithHint(
            "##filename", "File name", inputNameBuffer_.data(), inputNameBuffer_.size(),
            ImGuiInputTextFlags_CallbackResize, ExpandInputBuffer, &inputNameBuffer_) &&
           inputNameBuffer_[0] != '\0')
        {
            selectedFilenames_ = { u8StrToPath(inputNameBuffer_.data()) };
        }
        focusOnInputText |= IsItemFocused();
        PopItemWidth();

        if(showFilterHere)
        {
            SameLine();
            PushItemWidth(filterW);
            if(BeginCombo("##type_filters", typeFilters_[typeFilterIndex_].c_str()))
            {
                ScopeGuard guard([&] { EndCombo(); });
                for(size_t i = 0; i < typeFilters_.size(); ++i)
                {
                    const bool selected = i == typeFilterIndex_;
                    if(Selectable(typeFilters_[i].c_str(), selected) && !selected)
                    {
                        typeFilterIndex_ = static_cast<unsigned int>(i);
                    }
                }
            }
            PopItemWidth();
        }
    }

    if(!focusOnInputText && !editDir_)
    {
        const bool selectAll = (flags_ & ImGuiFileBrowserFlags_MultipleSelection) &&
                               IsKeyPressed(ImGuiKey_A) && (IsKeyDown(ImGuiKey_LeftCtrl) ||
                               IsKeyDown(ImGuiKey_RightCtrl));
        if(selectAll)
        {
            const bool needDir = flags_ & ImGuiFileBrowserFlags_SelectDirectory;
            selectedFilenames_.clear();
            for(size_t i = 1; i < fileRecords_.size(); ++i)
            {
                auto &record = fileRecords_[i];
                if(record.isDir == needDir &&
                   (needDir || IsExtensionMatched(record.extension)))
                {
                    selectedFilenames_.insert(record.name);
                }
            }
        }
    }

    const bool isEnterPressed =
        (flags_ & ImGuiFileBrowserFlags_ConfirmOnEnter) &&
        IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        IsKeyPressed(ImGuiKey_Enter);

    if(!(flags_ & ImGuiFileBrowserFlags_EnterNewFilename) && !typeFilters_.empty())
    {
        PushItemWidth(8.0f * GetFontSize());
        if(BeginCombo("##type_filters", typeFilters_[typeFilterIndex_].c_str()))
        {
            ScopeGuard guard([&] { EndCombo(); });
            for(size_t i = 0; i < typeFilters_.size(); ++i)
            {
                const bool selected = i == typeFilterIndex_;
                if(Selectable(typeFilters_[i].c_str(), selected) && !selected)
                {
                    typeFilterIndex_ = static_cast<unsigned int>(i);
                }
            }
        }
        PopItemWidth();
        SameLine();
    }

    if(listingBusy_.load(std::memory_order_acquire) &&
       !(flags_ & ImGuiFileBrowserFlags_NoStatusBar))
    {
        AlignTextToFramePadding();
        TextUnformatted("Reading folder...");
        SameLine();
    }
    else if(!statusStr_.empty() && !(flags_ & ImGuiFileBrowserFlags_NoStatusBar))
    {
        AlignTextToFramePadding();
        TextUnformatted(statusStr_.c_str());
        SameLine();
    }

    {
        const ImGuiStyle &st = GetStyle();
        const float minBtnW = GetFontSize() * 6.0f;
        const float okW = (std::max)(minBtnW, CalcTextSize("OK").x + st.FramePadding.x * 2.0f);
        const float cancelW = (std::max)(minBtnW, CalcTextSize("Cancel").x + st.FramePadding.x * 2.0f);
        const float pairW = okW + st.ItemSpacing.x + cancelW;
        SetCursorPosX(GetCursorPosX() + GetContentRegionAvail().x - pairW);

        bool okClicked = false;
        if(!(flags_ & ImGuiFileBrowserFlags_SelectDirectory))
        {
            okClicked = (Button("OK", ImVec2(okW, 0.0f)) || isEnterPressed) &&
                        !selectedFilenames_.empty();
        }
        else
        {
            okClicked = Button("OK", ImVec2(okW, 0.0f)) || isEnterPressed;
        }
        if(okClicked)
        {
            isOk_ = true;
            CloseCurrentPopup();
        }

        SameLine();
        const bool shouldClose =
            Button("Cancel", ImVec2(cancelW, 0.0f)) || shouldClose_ ||
            ((flags_ & ImGuiFileBrowserFlags_CloseOnEsc) &&
             IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
             IsKeyPressed(ImGuiKey_Escape));
        if(shouldClose)
        {
            CloseCurrentPopup();
        }
    }
}

inline bool ImGui::FileBrowser::HasSelected() const noexcept
{
    return isOk_;
}

inline bool ImGui::FileBrowser::SetDirectory(const std::filesystem::path &dir)
{
    const std::filesystem::path preferredFallback = this->GetDirectory();
    return SetCurrentDirectoryInternal(dir, preferredFallback);
}

inline const std::filesystem::path &ImGui::FileBrowser::GetDirectory() const noexcept
{
    return currentDirectory_;
}

inline std::filesystem::path ImGui::FileBrowser::GetSelected() const
{
    // when isOk_ is true, selectedFilenames_ may be empty if SelectDirectory
    // is enabled. return pwd in that case.
    if(selectedFilenames_.empty())
    {
        return currentDirectory_;
    }
    return currentDirectory_ / *selectedFilenames_.begin();
}

inline std::vector<std::filesystem::path> ImGui::FileBrowser::GetMultiSelected() const
{
    if(selectedFilenames_.empty())
    {
        return { currentDirectory_ };
    }

    std::vector<std::filesystem::path> ret;
    ret.reserve(selectedFilenames_.size());
    for(auto &s : selectedFilenames_)
    {
        ret.push_back(currentDirectory_ / s);
    }

    return ret;
}

inline void ImGui::FileBrowser::ClearSelected()
{
    selectedFilenames_.clear();
    if((flags_ & ImGuiFileBrowserFlags_EnterNewFilename))
    {
        AssignToArrayStyleString(inputNameBuffer_, "");
    }
    isOk_ = false;
}

inline void ImGui::FileBrowser::SetTypeFilters(const std::vector<std::string> &_typeFilters)
{
    typeFilters_.clear();

    // remove duplicate filter names due to case unsensitivity on windows

#ifdef _WIN32

    std::vector<std::string> typeFilters;
    for(auto &rawFilter : _typeFilters)
    {
        std::string lowerFilter = ToLower(rawFilter);
        const auto it = std::find(typeFilters.begin(), typeFilters.end(), lowerFilter);
        if(it == typeFilters.end())
        {
            typeFilters.push_back(std::move(lowerFilter));
        }
    }

#else

    auto &typeFilters = _typeFilters;

#endif

    // insert auto-generated filter
    hasAllFilter_ = false;
    if(typeFilters.size() > 1)
    {
        hasAllFilter_  = true;
        std::string allFiltersName = std::string();
        for(size_t i = 0; i < typeFilters.size(); ++i)
        {
            if(typeFilters[i] == std::string_view(".*"))
            {
                hasAllFilter_ = false;
                break;
            }

            if(i > 0)
            {
                allFiltersName += ",";
            }
            allFiltersName += typeFilters[i];
        }

        if(hasAllFilter_)
        {
            typeFilters_.push_back(std::move(allFiltersName));
        }
    }

    std::copy(typeFilters.begin(), typeFilters.end(), std::back_inserter(typeFilters_));
    typeFilterIndex_ = 0;
}

inline void ImGui::FileBrowser::SetCurrentTypeFilterIndex(int index)
{
    typeFilterIndex_ = static_cast<unsigned int>(index);
}

inline const std::string &ImGui::FileBrowser::GetCurrentTypeFilter() const noexcept
{
    static const std::string empty;
    if(typeFilters_.empty())
    {
        return empty;
    }
    if(static_cast<size_t>(typeFilterIndex_) >= typeFilters_.size())
    {
        return typeFilters_.front();
    }
    return typeFilters_[typeFilterIndex_];
}

inline void ImGui::FileBrowser::ClearQuickAccess()
{
    quickAccess_.clear();
}

inline void ImGui::FileBrowser::AddQuickAccess(std::string label, std::filesystem::path path)
{
    if(label.empty() || path.empty())
    {
        return;
    }
    std::error_code ec;
    if(!std::filesystem::is_directory(path, ec))
    {
        return;
    }
    quickAccess_.push_back(QuickAccessEntry{std::move(label), std::move(path)});
}

inline void ImGui::FileBrowser::SetQuickAccessWidth(float width) noexcept
{
    quickAccessWidth_ = width > 48.f ? width : 48.f;
}

inline void ImGui::FileBrowser::SetInputName(std::string_view input)
{
    assert((flags_ & ImGuiFileBrowserFlags_EnterNewFilename) &&
           "SetInputName can only be called when ImGuiFileBrowserFlags_EnterNewFilename is enabled");
    customizedInputName_ = input;
}

inline std::string ImGui::FileBrowser::ToLower(const std::string &s)
{
    std::string ret = s;
    for(char &c : ret)
    {
        c = static_cast<char>(std::tolower(c));
    }
    return ret;
}

inline void ImGui::FileBrowser::ToolTip(const std::string_view &s)
{
    if (!ImGui::IsItemHovered())
    {
        return;
    }
    ImGui::SetTooltip("%s", s.data());
}

inline std::string ImGui::FileBrowser::FormatByteSize(std::uint64_t bytes)
{
    if(bytes < 1024ull)
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%llu B", static_cast<unsigned long long>(bytes));
        return buf;
    }
    const char *units[] = { "KB", "MB", "GB", "TB" };
    double v = static_cast<double>(bytes);
    int u = -1;
    do
    {
        v /= 1024.0;
        ++u;
    } while(v >= 1024.0 && u < 3);
    char buf[32];
    std::snprintf(buf, sizeof(buf), (v >= 10.0) ? "%.0f %s" : "%.1f %s", v, units[u]);
    return buf;
}

inline std::string ImGui::FileBrowser::FormatFileTime(const std::filesystem::file_time_type &tp)
{
    try
    {
        const auto sys = std::chrono::clock_cast<std::chrono::system_clock>(tp);
        const std::time_t tt = std::chrono::system_clock::to_time_t(sys);
        const std::tm *tmp = std::localtime(&tt);
        if(!tmp)
        {
            return {};
        }
        const std::tm tm = *tmp;
        char buf[32];
        if(std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm) == 0)
        {
            return {};
        }
        return buf;
    }
    catch(...)
    {
        return {};
    }
}

inline int ImGui::FileBrowser::CompareRecordNames(const FileRecord &a, const FileRecord &b)
{
    const auto as = u8StrToStr(a.name.u8string());
    const auto bs = u8StrToStr(b.name.u8string());
    const size_t n = (std::min)(as.size(), bs.size());
    for(size_t i = 0; i < n; ++i)
    {
        const unsigned char ca = static_cast<unsigned char>(as[i]);
        const unsigned char cb = static_cast<unsigned char>(bs[i]);
        const unsigned char la = static_cast<unsigned char>(std::tolower(ca));
        const unsigned char lb = static_cast<unsigned char>(std::tolower(cb));
        if(la != lb)
        {
            return la < lb ? -1 : 1;
        }
        if(ca != cb)
        {
            return ca < cb ? -1 : 1;
        }
    }
    if(as.size() != bs.size())
    {
        return as.size() < bs.size() ? -1 : 1;
    }
    return 0;
}

inline void ImGui::FileBrowser::SortFileRecords(const ImGuiTableSortSpecs *sortSpecs)
{
    if(fileRecords_.size() <= 2)
    {
        return;
    }

    ImGuiID column = 0;
    bool descending = false;
    if(sortSpecs && sortSpecs->SpecsCount > 0)
    {
        column = sortSpecs->Specs[0].ColumnUserID;
        descending = sortSpecs->Specs[0].SortDirection == ImGuiSortDirection_Descending;
    }

    std::sort(fileRecords_.begin() + 1, fileRecords_.end(),
        [&](const FileRecord &a, const FileRecord &b)
        {
            if(a.isDir != b.isDir)
            {
                return a.isDir;
            }
            int cmp = 0;
            switch(column)
            {
            case 1:
                cmp = (a.size < b.size) ? -1 : (a.size > b.size) ? 1 : 0;
                break;
            case 2:
                cmp = a.typeText.compare(b.typeText);
                break;
            case 3:
                cmp = (a.lastWriteUnix < b.lastWriteUnix) ? -1 : (a.lastWriteUnix > b.lastWriteUnix) ? 1 : 0;
                break;
            default:
                break;
            }
            if(cmp == 0)
            {
                cmp = CompareRecordNames(a, b);
            }
            return descending ? (cmp > 0) : (cmp < 0);
        });
}

inline std::vector<ImGui::FileBrowser::FileRecord> ImGui::FileBrowser::CollectFileRecords(
    const std::filesystem::path &dir, ImGuiFileBrowserFlags flags)
{
    std::vector<FileRecord> records;
    FileRecord parent;
    parent.isDir = true;
    parent.name = "..";
    parent.showName = "..";
    parent.typeText = "Folder";
    records.push_back(std::move(parent));

    // skip_permission_denied: one unreadable entry must not abort the listing
    for(auto &p : std::filesystem::directory_iterator(
            dir, std::filesystem::directory_options::skip_permission_denied))
    {
        FileRecord rcd;
        try
        {
            if(p.is_regular_file())
            {
                rcd.isDir = false;
            }
            else if(p.is_directory())
            {
                rcd.isDir = true;
            }
            else
            {
                continue;
            }

            rcd.name = p.path().filename();
            if(rcd.name.empty())
            {
                continue;
            }

            rcd.extension = p.path().filename().extension();
            rcd.showName = u8StrToStr(p.path().filename().u8string());
            if(rcd.isDir)
            {
                rcd.typeText = "Folder";
            }
            else
            {
                std::string ext = u8StrToStr(rcd.extension.u8string());
                if(ext.empty())
                {
                    rcd.typeText = "File";
                }
                else
                {
                    if(ext.front() == '.')
                    {
                        ext.erase(0, 1);
                    }
                    for(char &c : ext)
                    {
                        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                    }
                    rcd.typeText = ext + " File";
                }
            }

            std::error_code ec;
            if(!rcd.isDir)
            {
                const auto sz = p.file_size(ec);
                if(!ec)
                {
                    rcd.size = static_cast<std::uint64_t>(sz);
                    rcd.sizeText = FormatByteSize(rcd.size);
                }
            }
            const auto ft = p.last_write_time(ec);
            if(!ec)
            {
                rcd.dateText = FormatFileTime(ft);
                try
                {
                    const auto sys = std::chrono::clock_cast<std::chrono::system_clock>(ft);
                    rcd.lastWriteUnix = static_cast<std::int64_t>(
                        std::chrono::system_clock::to_time_t(sys));
                }
                catch(...)
                {
                }
            }
        }
        catch(...)
        {
            if(!(flags & ImGuiFileBrowserFlags_SkipItemsCausingError))
            {
                throw;
            }
            continue;
        }
        records.push_back(std::move(rcd));
    }
    return records;
}

inline void ImGui::FileBrowser::ShowListingPlaceholder()
{
    FileRecord parent;
    parent.isDir = true;
    parent.name = "..";
    parent.showName = "..";
    parent.typeText = "Folder";
    fileRecords_ = { std::move(parent) };
    needSort_ = true;
}

inline void ImGui::FileBrowser::ReapListing()
{
    if(!listingBusy_.load(std::memory_order_acquire) && listingThread_.joinable())
    {
        listingThread_.join();
    }
}

inline void ImGui::FileBrowser::RequestListing()
{
    listingGen_.fetch_add(1, std::memory_order_acq_rel);
    if(listingBusy_.load(std::memory_order_acquire))
    {
        listingStale_.store(true, std::memory_order_release);
        return;
    }
    ReapListing();
    listingStale_.store(false, std::memory_order_release);
    const std::uint64_t gen = listingGen_.load(std::memory_order_acquire);
    const std::filesystem::path dir = listingPath_.empty() ? currentDirectory_ : listingPath_;
    const ImGuiFileBrowserFlags flags = flags_;
    listingBusy_.store(true, std::memory_order_release);
    listingThread_ = std::thread([this, dir, gen, flags]()
    {
        ListingResult r;
        r.generation = gen;
        try
        {
            // Follow symlinks/junctions to the real directory. Windows legacy
            // junctions (Documents\My Music, ...) deny listing on the link
            // itself but allow traversal, so listing the resolved target is
            // the only way in.
            std::error_code canonEc;
            const auto resolved = std::filesystem::canonical(dir, canonEc);
            r.directory = canonEc ? std::filesystem::absolute(dir) : resolved;
            r.records = CollectFileRecords(r.directory, flags);
        }
        catch(const std::exception &err)
        {
            r.error = err.what();
        }
        catch(...)
        {
            r.error = "unknown error";
        }
        {
            std::lock_guard<std::mutex> lock(listingMutex_);
            listingPending_ = std::move(r);
        }
        listingBusy_.store(false, std::memory_order_release);
    });
}

inline void ImGui::FileBrowser::PumpListing()
{
    ReapListing();
    ListingResult pending;
    {
        std::lock_guard<std::mutex> lock(listingMutex_);
        if(listingPending_.generation != 0)
        {
            pending = std::move(listingPending_);
            listingPending_ = {};
        }
    }
    if(pending.generation != 0 &&
       pending.generation == listingGen_.load(std::memory_order_acquire))
    {
        if(pending.error.empty())
        {
            currentDirectory_ = std::move(pending.directory);
            fileRecords_ = std::move(pending.records);
            needSort_ = true;
            SortFileRecords(nullptr);
            ClearRangeSelectionState();
            statusStr_.clear();
        }
        else
        {
            statusStr_ = std::string("error: ") + pending.error;
            if(!listingFallback_.empty() && listingFallback_ != listingPath_)
            {
                listingPath_ = listingFallback_;
                currentDirectory_ = listingFallback_;
                listingFallback_.clear();
                RequestListing();
            }
        }
    }
    if(!listingBusy_.load(std::memory_order_acquire) &&
       listingStale_.load(std::memory_order_acquire))
    {
        RequestListing();
    }
}

inline void ImGui::FileBrowser::UpdateFileRecords()
{
    listingPath_ = currentDirectory_;
    RequestListing();
}

inline void ImGui::FileBrowser::SetCurrentDirectoryUncatched(const std::filesystem::path &pwd)
{
    listingPath_ = pwd;
    currentDirectory_ = pwd;
    ShowListingPlaceholder();

    bool shouldClearInputNameBuffer = true;

    if((flags_ & ImGuiFileBrowserFlags_EnterNewFilename) &&
       selectedFilenames_.size() == 1 &&
       !customizedInputName_.empty() &&
       !inputNameBuffer_.empty() &&
       std::strcmp(inputNameBuffer_.data(), customizedInputName_.data()) == 0)
    {
        shouldClearInputNameBuffer = false;
    }

    if(shouldClearInputNameBuffer)
    {
        selectedFilenames_.clear();
        AssignToArrayStyleString(inputNameBuffer_, "");
    }

    RequestListing();
}

inline bool ImGui::FileBrowser::SetCurrentDirectoryInternal(
    const std::filesystem::path &dir, const std::filesystem::path &preferredFallback)
{
    listingFallback_ = preferredFallback;
    SetCurrentDirectoryUncatched(dir);
    return true;
}

inline bool ImGui::FileBrowser::IsExtensionMatched(const std::filesystem::path &_extension) const
{
#ifdef _WIN32
    std::filesystem::path extension = ToLower(u8StrToStr(_extension.u8string()));
#else
    auto &extension = _extension;
#endif

    // no type filters
    if(typeFilters_.empty())
    {
        return true;
    }

    // invalid type filter index
    if(static_cast<size_t>(typeFilterIndex_) >= typeFilters_.size())
    {
        return true;
    }

    // all type filters
    if(hasAllFilter_ && typeFilterIndex_ == 0)
    {
        for(size_t i = 1; i < typeFilters_.size(); ++i)
        {
            if(extension == typeFilters_[i])
            {
                return true;
            }
        }
        return false;
    }

    // universal filter
    if(typeFilters_[typeFilterIndex_] == std::string_view(".*"))
    {
        return true;
    }

    // regular filter
    return extension == typeFilters_[typeFilterIndex_];
}

inline void ImGui::FileBrowser::ClearRangeSelectionState()
{
    rangeSelectionStart_ = 9999999;
    const bool dir = flags_ & ImGuiFileBrowserFlags_SelectDirectory;
    for(unsigned int i = 1; i < fileRecords_.size(); ++i)
    {
        if(fileRecords_[i].isDir == dir)
        {
            if(!dir && !IsExtensionMatched(fileRecords_[i].extension))
            {
                continue;
            }
            rangeSelectionStart_ = i;
            break;
        }
    }
}

inline void ImGui::FileBrowser::AssignToArrayStyleString(std::vector<char> &arr, std::string_view content)
{
    if(content.empty())
    {
        if(!arr.empty())
        {
            arr[0] = '\0';
        }
        return;
    }

    if(arr.size() < content.size() + 1)
    {
        arr.resize(content.size() + 1);
    }
    std::memcpy(arr.data(), content.data(), content.size());
    arr[content.size()] = '\0';
}

inline int ImGui::FileBrowser::ExpandInputBuffer(ImGuiInputTextCallbackData *callbackData)
{
    if(callbackData && callbackData->EventFlag & ImGuiInputTextFlags_CallbackResize)
    {
        auto buffer = static_cast<std::vector<char>*>(callbackData->UserData);
        size_t newSize = buffer->size();
        while(newSize < static_cast<size_t>(callbackData->BufSize))
        {
            newSize <<= 1;
        }
        buffer->resize(newSize, '\0');
        callbackData->Buf = buffer->data();
        callbackData->BufDirty = true;
    }
    return 0;
}

#if defined(__cpp_lib_char8_t)
inline std::string ImGui::FileBrowser::u8StrToStr(std::u8string s)
{
    std::string result;
    result.resize(s.length());
    std::memcpy(result.data(), s.data(), s.length());
    return result;
}
#endif

inline std::string ImGui::FileBrowser::u8StrToStr(std::string s)
{
    return s;
}

inline std::filesystem::path ImGui::FileBrowser::u8StrToPath(const char *str)
{
#if defined(__cpp_lib_char8_t)
    // With C++20/23, it's impossible to efficiently convert a `char*` string to a `char8_t*` string without violating
    // the strict aliasing rule. Bad joke!
    const size_t len = std::strlen(str);
    std::u8string u8Str;
    u8Str.resize(len);
    std::memcpy(u8Str.data(), str, len);
    return std::filesystem::path(u8Str);
#else
    // u8path is deprecated in C++20
    return std::filesystem::u8path(str);
#endif
}

inline ImGui::FileBrowser::~FileBrowser()
{
    if(listingThread_.joinable())
    {
        listingThread_.join();
    }
#ifdef _WIN32
    if(driveScanThread_.joinable())
    {
        driveScanThread_.join();
    }
#endif
}

#ifdef _WIN32

inline void ImGui::FileBrowser::ReapDriveScan()
{
    // Never join a running scan on the UI thread — GetDriveTypeA can block
    // for seconds while Windows mounts a USB stick.
    if(!driveScanBusy_.load(std::memory_order_acquire) && driveScanThread_.joinable())
    {
        driveScanThread_.join();
    }
}

inline void ImGui::FileBrowser::RequestDriveScan(bool force)
{
    if(driveScanBusy_.load(std::memory_order_acquire))
    {
        return;
    }
    ReapDriveScan();
    const double now = GetTime();
    if(!force && lastDriveScanTime_ >= 0.0 && (now - lastDriveScanTime_) < 0.5)
    {
        return;
    }
    lastDriveScanTime_ = now;
    driveScanBusy_.store(true, std::memory_order_release);
    driveScanThread_ = std::thread([this]()
    {
        const std::uint32_t mask = GetDrivesBitMask();
        for(int i = 0; i < 26; ++i)
        {
            unsigned int t = 0;
            if(mask & (1u << i))
            {
                const char root[] = { static_cast<char>('A' + i), ':', '\\', '\0' };
                t = GetDriveTypeA(root);
            }
            driveType_[i].store(t, std::memory_order_relaxed);
        }
        drivesLive_.store(mask, std::memory_order_release);
        driveScanBusy_.store(false, std::memory_order_release);
    });
}

inline std::uint32_t ImGui::FileBrowser::GetDrivesBitMask()
{
    // GetLogicalDrives is the assigned-letter mask. std::filesystem::exists("E:\\")
    // throws or returns false for removable volumes that are not fully ready yet,
    // which is why a USB stick often never appeared in the combo / sidebar.
    return static_cast<std::uint32_t>(GetLogicalDrives());
}

#endif
