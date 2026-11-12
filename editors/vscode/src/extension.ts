import * as path from 'path';
import * as fs from 'fs';
import * as cp from 'child_process';
import * as vscode from 'vscode';
import {
    LanguageClient,
    LanguageClientOptions,
    ServerOptions,
    TransportKind
} from 'vscode-languageclient/node';

let client: LanguageClient | undefined;
let doctorChannel: vscode.OutputChannel | undefined;

function findCompilerBinary(): string {
    const config = vscode.workspace.getConfiguration('santientphp');
    const configuredPath = config.get<string>('compilerPath');

    if (configuredPath && configuredPath !== 'sphc' && configuredPath !== 'sphc.exe') {
        if (path.isAbsolute(configuredPath) && fs.existsSync(configuredPath)) {
            return configuredPath;
        }
        if (vscode.workspace.workspaceFolders && vscode.workspace.workspaceFolders.length > 0) {
            const root = vscode.workspace.workspaceFolders[0].uri.fsPath;
            const fullPath = path.resolve(root, configuredPath);
            if (fs.existsSync(fullPath)) {
                return fullPath;
            }
        }
    }

    // Check workspace build directories
    if (vscode.workspace.workspaceFolders && vscode.workspace.workspaceFolders.length > 0) {
        const root = vscode.workspace.workspaceFolders[0].uri.fsPath;
        const candidates = [
            path.join(root, 'build', 'bin', 'sphc.exe'),
            path.join(root, 'build', 'bin', 'sphc'),
            path.join(root, 'bin', 'sphc.exe'),
            path.join(root, 'bin', 'sphc')
        ];
        for (const cand of candidates) {
            if (fs.existsSync(cand)) {
                return cand;
            }
        }
    }

    return process.platform === 'win32' ? 'sphc.exe' : 'sphc';
}

function createClient(compilerPath: string): LanguageClient {
    const serverOptions: ServerOptions = {
        command: compilerPath,
        args: ['lsp'],
        transport: TransportKind.stdio
    };

    const clientOptions: LanguageClientOptions = {
        documentSelector: [{ scheme: 'file', language: 'santientphp' }],
        synchronize: {
            fileEvents: vscode.workspace.createFileSystemWatcher('**/*.sph')
        },
        outputChannelName: 'SantientPHP Language Server'
    };

    return new LanguageClient(
        'santientphp',
        'SantientPHP Language Server',
        serverOptions,
        clientOptions
    );
}

export async function activate(context: vscode.ExtensionContext) {
    const compilerPath = findCompilerBinary();

    // 1. Start Language Server Client
    try {
        client = createClient(compilerPath);
        await client.start();
    } catch (err) {
        vscode.window.showErrorMessage(
            `Failed to start SantientPHP Language Server with binary '${compilerPath}': ${err}`
        );
    }

    // 2. Command: Restart Server
    const restartCmd = vscode.commands.registerCommand('santientphp.restartServer', async () => {
        if (client) {
            try {
                await client.stop();
            } catch {
                // Ignore stop error
            }
        }
        const newBinary = findCompilerBinary();
        client = createClient(newBinary);
        try {
            await client.start();
            vscode.window.showInformationMessage('SantientPHP Language Server restarted successfully.');
        } catch (err) {
            vscode.window.showErrorMessage(`Failed to restart Language Server: ${err}`);
        }
    });

    // 3. Command: Doctor Diagnostics
    const doctorCmd = vscode.commands.registerCommand('santientphp.doctor', () => {
        if (!doctorChannel) {
            doctorChannel = vscode.window.createOutputChannel('SantientPHP Doctor');
        }
        doctorChannel.clear();
        doctorChannel.show(true);
        doctorChannel.appendLine('Running SantientPHP Doctor diagnostics...\n');

        const activeCompiler = findCompilerBinary();
        cp.exec(`"${activeCompiler}" doctor`, (error, stdout, stderr) => {
            if (stdout) {
                doctorChannel?.appendLine(stdout);
            }
            if (stderr) {
                doctorChannel?.appendLine(stderr);
            }
            if (error) {
                doctorChannel?.appendLine(`\nProcess exited with error code ${error.code}`);
            }
        });
    });

    // 4. Command: Build
    const buildCmd = vscode.commands.registerCommand('santientphp.build', () => {
        const editor = vscode.window.activeTextEditor;
        const activeCompiler = findCompilerBinary();
        const targetPath = editor && editor.document.fileName.endsWith('.sph')
            ? editor.document.fileName
            : '';

        const term = vscode.window.createTerminal('SantientPHP Build');
        term.show();
        if (targetPath) {
            term.sendText(`& "${activeCompiler}" build "${targetPath}"`);
        } else {
            term.sendText(`& "${activeCompiler}" build`);
        }
    });

    // 5. Command: Run
    const runCmd = vscode.commands.registerCommand('santientphp.run', () => {
        const editor = vscode.window.activeTextEditor;
        if (!editor || !editor.document.fileName.endsWith('.sph')) {
            vscode.window.showWarningMessage('Please open a .sph file to run.');
            return;
        }
        const activeCompiler = findCompilerBinary();
        const term = vscode.window.createTerminal('SantientPHP Run');
        term.show();
        term.sendText(`& "${activeCompiler}" run "${editor.document.fileName}"`);
    });

    // 6. Command & Provider: Document Formatting
    const formatCmd = vscode.commands.registerCommand('santientphp.format', async () => {
        const editor = vscode.window.activeTextEditor;
        if (!editor || editor.document.languageId !== 'santientphp') return;

        const activeCompiler = findCompilerBinary();
        const filePath = editor.document.fileName;
        if (fs.existsSync(filePath)) {
            cp.exec(`"${activeCompiler}" fmt --stdout "${filePath}"`, (err, stdout) => {
                if (!err && stdout) {
                    const fullRange = new vscode.Range(
                        editor.document.positionAt(0),
                        editor.document.positionAt(editor.document.getText().length)
                    );
                    editor.edit(editBuilder => editBuilder.replace(fullRange, stdout));
                }
            });
        }
    });

    // Register DocumentFormattingEditProvider
    const formattingProvider = vscode.languages.registerDocumentFormattingEditProvider('santientphp', {
        provideDocumentFormattingEdits(document: vscode.TextDocument): Promise<vscode.TextEdit[]> {
            return new Promise((resolve) => {
                const activeCompiler = findCompilerBinary();
                const filePath = document.fileName;
                if (!fs.existsSync(filePath)) {
                    return resolve([]);
                }
                cp.exec(`"${activeCompiler}" fmt --stdout "${filePath}"`, (err, stdout) => {
                    if (err || !stdout) {
                        return resolve([]);
                    }
                    const fullRange = new vscode.Range(
                        document.positionAt(0),
                        document.positionAt(document.getText().length)
                    );
                    resolve([vscode.TextEdit.replace(fullRange, stdout)]);
                });
            });
        }
    });

    context.subscriptions.push(
        restartCmd,
        doctorCmd,
        buildCmd,
        runCmd,
        formatCmd,
        formattingProvider
    );
}

export async function deactivate(): Promise<void> {
    if (client) {
        await client.stop();
    }
}

