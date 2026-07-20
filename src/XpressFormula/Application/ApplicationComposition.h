// ApplicationComposition.h - Directly-owned services for the Win32 application host.
#pragma once

#include "../Infrastructure/Persistence/ProjectRepository.h"
#include "../Infrastructure/Persistence/RecentProjectsStore.h"
#include "../Platform/Windows/ClipboardService.h"
#include "../Platform/Windows/FileDialogService.h"
#include "../Platform/Windows/ShellService.h"
#include "../Platform/Windows/WicImageEncoder.h"

namespace XpressFormula::Application {

struct ApplicationComposition {
    Infrastructure::Persistence::ProjectRepository projectRepository;
    Infrastructure::Persistence::RecentProjectsStore recentProjectsStore;
    Platform::Windows::FileDialogService fileDialogService;
    Platform::Windows::ShellService shellService;
    Platform::Windows::ClipboardService clipboardService;
    Platform::Windows::WicImageEncoder imageEncoder;
};

} // namespace XpressFormula::Application
