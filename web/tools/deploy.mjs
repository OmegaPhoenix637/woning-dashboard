// Bouwt de site en zet hem op de gh-pages-branch (GitHub Pages: "Deploy from a branch").
// Gebruik:  npm run deploy
import { execSync } from 'node:child_process';
import { writeFileSync, rmSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const dist = path.join(root, 'dist');
const run = (cmd, cwd = root) => execSync(cmd, { cwd, stdio: 'inherit' });
const out = (cmd, cwd = root) => execSync(cmd, { cwd }).toString().trim();

const remote = out('git remote get-url origin');
const rev = out('git rev-parse --short HEAD');

run('npm test');
run('npm run build');

writeFileSync(path.join(dist, '.nojekyll'), '');
rmSync(path.join(dist, '.git'), { recursive: true, force: true });
run('git init -q -b gh-pages', dist);
run('git add -A', dist);
run(`git -c user.name="deploy" -c user.email="deploy@users.noreply.github.com" commit -q -m "Site bijgewerkt (${rev})"`, dist);
run(`git push -q -f ${remote} gh-pages`, dist);
rmSync(path.join(dist, '.git'), { recursive: true, force: true });
console.log('✓ Gepubliceerd op gh-pages');
