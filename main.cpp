#include <iostream>
#include <cstdlib>
#include <vector>
#include <cstdio>

// Function to execute shell commands and check for errors
void executeCommand(const std::string& command) {
    std::cout << "\033[38;2;255;215;0m"; // Set text color to gold
    int result = system(command.c_str()); // No automatic sudo here
    if (result != 0) {
        std::cerr << "Command failed: " << command << std::endl;
        exit(EXIT_FAILURE);
    }
}

// Function to fetch UUID of a partition
std::string fetchUUID(const std::string& partition) {
    std::string command = "sudo blkid -s UUID -o value " + partition; // Explicit sudo
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        std::cerr << "Failed to fetch UUID for partition: " << partition << std::endl;
        exit(EXIT_FAILURE);
    }

    char buffer[128];
    std::string uuid;
    if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        uuid = buffer;
        uuid.erase(uuid.find_last_not_of(" \n\r\t") + 1); // Remove trailing newline
    }
    pclose(pipe);

    if (uuid.empty()) {
        std::cerr << "Failed to fetch UUID for partition: " << partition << std::endl;
        exit(EXIT_FAILURE);
    }

    return uuid;
}

// Function to check for SquashFS file in default locations
std::string findSquashFS() {
    std::vector<std::string> defaultLocations = {
        "/run/live/medium/live/filesystem.squashfs", // Common Debian Live path
        "/live/image/live/filesystem.squashfs",      // Alternate Debian Live path
        "/run/live/medium/live/filesystem.sfs"       // Fallback
    };

    for (const auto& location : defaultLocations) {
        if (system(("sudo test -f " + location).c_str()) == 0) { // Explicit sudo
            return location;
        }
    }
    return "";
}

// Function to display the menu
void displayMenu(const std::string& TARGET_PARTITION) {
    std::string choice;
    std::cout << "\033[38;2;255;215;0m\n=== Menu ===\033[0m\n";
    std::cout << "\033[38;2;255;215;0m1. Chroot into the new system\033[0m\n";
    std::cout << "\033[38;2;255;215;0m2. Reboot the system\033[0m\n";
    std::cout << "\033[38;2;255;215;0m3. Exit\033[0m\n";
    std::cout << "\033[38;2;255;215;0mEnter your choice (1/2/3): \033[0m";
    std::getline(std::cin, choice);

    if (choice == "1") {
        // Chroot into the new system
        std::cout << "\033[38;2;255;215;0mChrooting into the new system...\033[0m\n";
        executeCommand("sudo mount " + TARGET_PARTITION + "2 /mnt"); // Explicit sudo
        executeCommand("sudo mount --bind /dev /mnt/dev"); // Explicit sudo
        executeCommand("sudo mount --bind /dev/pts /mnt/dev/pts"); // Explicit sudo
        executeCommand("sudo mount --bind /sys /mnt/sys"); // Explicit sudo
        executeCommand("sudo mount --bind /proc /mnt/proc"); // Explicit sudo
        executeCommand("sudo chroot /mnt /bin/bash"); // Explicit sudo
    } else if (choice == "2") {
        // Reboot the system
        std::cout << "\033[38;2;255;215;0mRebooting the system...\033[0m\n";
        executeCommand("sudo reboot"); // Explicit sudo
    } else if (choice == "3") {
        // Exit the script
        std::cout << "\033[38;2;255;215;0mExiting...\033[0m\n";
        exit(EXIT_SUCCESS);
    } else {
        std::cerr << "\033[38;2;255;215;0mInvalid choice. Exiting.\033[0m\n";
        exit(EXIT_FAILURE);
    }
}

int main() {
    // Set terminal text color to gold for the entire script
    std::cout << "\033[38;2;255;215;0m";

    // Display version text
    std::cout << "Debian UEFI Ext4 Installer v1.0\n";

    // Ensure the script is run as root
    if (system("sudo [[ $(id -u) -ne 0 ]]") == 0) { // Explicit sudo
        std::cerr << "Please run as root\n";
        return EXIT_FAILURE;
    }

    // Ask for the target partition
    std::string TARGET_PARTITION;
    std::cout << "Enter the target partition (e.g., /dev/sdb): ";
    std::getline(std::cin, TARGET_PARTITION);

    // Confirm the partition
    std::string CONFIRM;
    std::cout << "You selected " << TARGET_PARTITION
    << ". This will wipe all data on this partition. Continue? (y/n): ";
    std::getline(std::cin, CONFIRM);
    if (CONFIRM != "y") {
        std::cout << "Operation canceled.\n";
        return EXIT_SUCCESS;
    }

    // Ask if the user wants to search default SquashFS locations
    std::string SEARCH_DEFAULT;
    std::cout << "Do you want to search default SquashFS locations? (y/n): ";
    std::getline(std::cin, SEARCH_DEFAULT);

    std::string squashfs_path;
    if (SEARCH_DEFAULT == "y") {
        // Search default locations
        std::cout << "Checking for SquashFS file in default locations...\n";
        squashfs_path = findSquashFS();
        if (squashfs_path.empty()) {
            std::cerr << "SquashFS file not found in default locations.\n";
            return EXIT_FAILURE;
        }
        std::cout << "Found SquashFS file at: " << squashfs_path << "\n";
    } else {
        // Ask for custom SquashFS location
        std::cout << "Enter the path to the SquashFS file: ";
        std::getline(std::cin, squashfs_path);
    }

    // Verify SquashFS file exists
    if (system(("sudo test -f " + squashfs_path).c_str()) != 0) { // Explicit sudo
        std::cerr << "Invalid path. Exiting.\n";
        return EXIT_FAILURE;
    }

    // Unmount all partitions on the target device
    std::cout << "Unmounting all partitions on " << TARGET_PARTITION << "...\n";
    std::string unmount_command = "sudo bash -c 'for PARTITION in $(lsblk -lnpo NAME " + TARGET_PARTITION + " | grep -oP \"^.*(?=\\s)\"); do sudo umount $PARTITION 2>/dev/null; done'";
    executeCommand(unmount_command); // Fixed for loop syntax

    // Wipe filesystem signatures and partition table
    std::cout << "Wiping filesystem signatures and partition table on " << TARGET_PARTITION << "...\n";
    executeCommand("sudo wipefs --all " + TARGET_PARTITION); // Explicit sudo

    // Create a new GPT partition table
    std::cout << "Creating new GPT partition table...\n";
    executeCommand("sudo parted -s " + TARGET_PARTITION + " mklabel gpt"); // Explicit sudo

    // Create partitions
    std::cout << "Creating partitions...\n";
    executeCommand("sudo parted -s " + TARGET_PARTITION + " mkpart primary fat32 1MiB 551MiB"); // Explicit sudo
    executeCommand("sudo parted -s " + TARGET_PARTITION + " set 1 esp on"); // Explicit sudo
    executeCommand("sudo parted -s " + TARGET_PARTITION + " mkpart primary ext4 551MiB 100%"); // Explicit sudo

    // Format partitions
    std::cout << "Formatting partitions...\n";
    executeCommand("sudo mkfs.vfat " + TARGET_PARTITION + "1"); // Explicit sudo
    executeCommand("sudo mkfs.ext4 " + TARGET_PARTITION + "2"); // Explicit sudo

    // Fetch UUIDs of the EFI and root partitions
    std::string efi_uuid = fetchUUID(TARGET_PARTITION + "1");
    std::string root_uuid = fetchUUID(TARGET_PARTITION + "2");

    // Mount partitions
    std::cout << "Mounting partitions...\n";
    executeCommand("sudo mount " + TARGET_PARTITION + "2 /mnt"); // Explicit sudo
    executeCommand("sudo mkdir -p /mnt/boot/efi"); // Explicit sudo
    executeCommand("sudo mount " + TARGET_PARTITION + "1 /mnt/boot/efi"); // Explicit sudo

    // Extract the SquashFS file
    std::cout << "Installing System...\n";
    executeCommand("sudo unsquashfs -f -d /mnt " + squashfs_path); // Explicit sudo

    // Generate fstab
    std::cout << "Generating fstab...\n";
    std::string fstab_content =
    "# /etc/fstab: static file system information.\n"
    "#\n"
    "# Use 'blkid' to print the universally unique identifier for a device.\n"
    "#\n"
    "# <file system> <mount point>   <type>  <options>       <dump>  <pass>\n"
    "UUID=" + root_uuid + " /               ext4    errors=remount-ro 0       1\n"
    "UUID=" + efi_uuid + "  /boot/efi       vfat    umask=0077      0       2\n";

    // Write fstab content to /mnt/etc/fstab
    std::string fstab_path = "/mnt/etc/fstab";
    std::string write_fstab_command = "echo '" + fstab_content + "' | sudo tee " + fstab_path; // Explicit sudo
    executeCommand(write_fstab_command);

    // Mount necessary filesystems for chroot
    executeCommand("sudo mount --bind /dev /mnt/dev"); // Mount /dev
    executeCommand("sudo mount --bind /dev/pts /mnt/dev/pts"); // Mount /dev/pts
    executeCommand("sudo mount --bind /sys /mnt/sys"); // Mount /sys
    executeCommand("sudo mount --bind /proc /mnt/proc"); // Mount /proc
    executeCommand("sudo mount --bind /run /mnt/run"); // Mount /run

    // Bind mount the live media
    executeCommand("sudo mkdir -p /mnt/run/live/medium"); // Create the directory
    executeCommand("sudo mount --bind /run/live/medium /mnt/run/live/medium"); // Bind mount live media

    // Chroot into the new system and update initramfs
    executeCommand("sudo chroot /mnt /bin/bash <<'EOF'\n"
    "sudo mount -t efivarfs efivarfs /sys/firmware/efi/efivars\n" // Mount EFI variables
    "sudo update-initramfs -u -k all\n" // Update initramfs
    "sudo grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=Debian --recheck\n" // Install GRUB
    "sudo update-grub\n" // Update GRUB configuration
    "EOF");

    // Unmount everything
    std::cout << "Unmounting partitions...\n";
    executeCommand("sudo umount /mnt/boot/efi"); // Explicit sudo
    executeCommand("sudo umount /mnt/dev/pts"); // Explicit sudo
    executeCommand("sudo umount /mnt/dev"); // Explicit sudo
    executeCommand("sudo umount -l /mnt/sys"); // Explicit sudo
    executeCommand("sudo umount /mnt/proc"); // Explicit sudo
    executeCommand("sudo umount /mnt/run/live/medium"); // Unmount live media
    executeCommand("sudo umount /mnt/run"); // Explicit sudo (Added /run to unbind)
    executeCommand("sudo umount -l /mnt"); // Explicit sudo

    // Display the menu
    displayMenu(TARGET_PARTITION);

    // Reset terminal text color to default
    std::cout << "\033[0m";

    return EXIT_SUCCESS;
}
