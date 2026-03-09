const fs = require('node:fs');
const path = require('node:path');

const rootDir = path.resolve(__dirname, '..');
const distDir = path.join(rootDir, 'dist');
const docsDir = path.join(rootDir, 'docs');
const packageJsonPath = path.join(rootDir, 'package.json');

const pkg = JSON.parse(fs.readFileSync(packageJsonPath, 'utf8'));

const assets = [
  'opensubdiv.js',
  'opensubdiv.mjs',
  'opensubdiv.wasm',
  'threejs.js',
];

fs.mkdirSync(docsDir, { recursive: true });

for (const asset of assets) {
  const sourcePath = path.join(distDir, asset);
  const targetPath = path.join(docsDir, asset);

  if (!fs.existsSync(sourcePath)) {
    throw new Error(`Missing build artifact: ${path.relative(rootDir, sourcePath)}`);
  }

  fs.copyFileSync(sourcePath, targetPath);
}

const metadata = {
  packageName: pkg.name,
  packageVersion: pkg.version,
  assetVersion: pkg.version,
  threeVersion: String(pkg.devDependencies?.three ?? '').replace(/^[^\d]*/, ''),
  builtAt: new Date().toISOString(),
};

const demoHtmlPath = path.join(docsDir, 'index.html');
const demoHtml = fs.readFileSync(demoHtmlPath, 'utf8').replace(
  /three@\d+\.\d+\.\d+/g,
  `three@${metadata.threeVersion}`
);

fs.writeFileSync(demoHtmlPath, demoHtml);

fs.writeFileSync(
  path.join(docsDir, 'demo-meta.json'),
  `${JSON.stringify(metadata, null, 2)}\n`
);
