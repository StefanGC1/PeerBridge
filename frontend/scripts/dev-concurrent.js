const { createServer } = require('vite');
const { spawn } = require('child_process');

(async () => {
    const viteServer = await createServer({
        configFile: './vite.config.mjs',
        root: process.cwd(),
        server: {open: false},
        // mode: 'development',
    });
    await viteServer.listen();

    const port = viteServer.config.server.port;
    const url = `http://localhost:${port}`;
    console.log(`Vite server is running on ${url}\n`);

    const electronPath = require('electron');
    const env = Object.assign({}, process.env, { VITE_URL: url });
    const electronProcess = spawn(electronPath, ['./'], {
        stdio: 'inherit',
        env: env,
    });

    electronProcess.on('close', (code) => {
        viteServer.close();
        process.exit(code);
    });
})();