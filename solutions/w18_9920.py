// fix-encrypted-folders.js
// Fix for AppImage encrypted folders "could not be located" error

const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

class EncryptedFolderFix {
    constructor() {
        this.appImagePath = process.env.APPIMAGE || '';
        this.configPath = path.join(process.env.HOME, '.config', 'Nextcloud');
        this.encryptedFolderPath = '';
    }

    async fix() {
        try {
            console.log('Starting encrypted folder fix...');
            
            // Detect AppImage environment
            if (!this.appImagePath) {
                console.log('Not running in AppImage environment, skipping fix');
                return;
            }

            // Find encrypted folders
            const encryptedFolders = this.findEncryptedFolders();
            
            if (encryptedFolders.length === 0) {
                console.log('No encrypted folders found');
                return;
            }

            // Fix each encrypted folder
            for (const folder of encryptedFolders) {
                await this.fixEncryptedFolder(folder);
            }

            console.log('Fix completed successfully');
        } catch (error) {
            console.error('Error during fix:', error);
            throw error;
        }
    }

    findEncryptedFolders() {
        const configFile = path.join(this.configPath, 'config.json');
        if (!fs.existsSync(configFile)) {
            console.log('Config file not found');
            return [];
        }

        const config = JSON.parse(fs.readFileSync(configFile, 'utf8'));
        const encryptedFolders = [];

        // Check for encrypted folders in sync configuration
        if (config.sync && config.sync.folders) {
            for (const folder of config.sync.folders) {
                if (folder.encrypted) {
                    encryptedFolders.push(folder);
                }
            }
        }

        return encryptedFolders;
    }

    async fixEncryptedFolder(folder) {
        console.log(`Fixing encrypted folder: ${folder.path}`);
        
        const folderPath = folder.path;
        const metadataPath = path.join(folderPath, '.metadata');
        
        // Check if metadata directory exists
        if (!fs.existsSync(metadataPath)) {
            console.log(`Creating metadata directory for ${folderPath}`);
            fs.mkdirSync(metadataPath, { recursive: true });
        }

        // Fix file permissions
        try {
            execSync(`chmod -R 755 "${folderPath}"`, { stdio: 'ignore' });
            console.log(`Fixed permissions for ${folderPath}`);
        } catch (error) {
            console.warn(`Could not fix permissions for ${folderPath}: ${error.message}`);
        }

        // Create proper symlinks for encrypted files
        const files = fs.readdirSync(folderPath);
        for (const file of files) {
            const filePath = path.join(folderPath, file);
            if (fs.statSync(filePath).isFile() && !file.startsWith('.')) {
                await this.fixEncryptedFile(filePath, folderPath);
            }
        }
    }

    async fixEncryptedFile(filePath, folderPath) {
        const fileName = path.basename(filePath);
        const metadataFile = path.join(folderPath, '.metadata', `${fileName}.metadata`);
        
        // Check if metadata file exists
        if (!fs.existsSync(metadataFile)) {
            console.log(`Creating metadata for ${fileName}`);
            
            // Create basic metadata
            const metadata = {
                fileName: fileName,
                encrypted: true,
                timestamp: new Date().toISOString(),
                version: 1
            };
            
            fs.writeFileSync(metadataFile, JSON.stringify(metadata, null, 2));
        }

        // Fix file path references in metadata
        try {
            const metadata = JSON.parse(fs.readFileSync(metadataFile, 'utf8'));
            
            // Update file path if needed
            if (metadata.filePath !== filePath) {
                metadata.filePath = filePath;
                fs.writeFileSync(metadataFile, JSON.stringify(metadata, null, 2));
                console.log(`Updated file path for ${fileName}`);
            }
        } catch (error) {
            console.warn(`Could not fix metadata for ${fileName}: ${error.message}`);
        }
    }

    // Additional fix for AppImage specific issues
    async fixAppImageSpecific() {
        // Fix for AppImage mount point issues
        const appImageMount = '/tmp/.mount_Nextcloud';
        if (fs.existsSync(appImageMount)) {
            try {
                execSync(`chmod -R 755 "${appImageMount}"`, { stdio: 'ignore' });
                console.log('Fixed AppImage mount permissions');
            } catch (error) {
                console.warn('Could not fix AppImage mount permissions');
            }
        }
    }
}

// Main execution
async function main() {
    const fix = new EncryptedFolderFix();
    try {
        await fix.fix();
        await fix.fixAppImageSpecific();
        console.log('Encrypted folder fix completed');
    } catch (error) {
        console.error('Failed to fix encrypted folders:', error);
        process.exit(1);
    }
}

// Run the fix
main();

// Export for testing
module.exports = EncryptedFolderFix;
