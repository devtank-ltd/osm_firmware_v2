export function move_bar(interval, text) {
    const progress = document.getElementById('progress');
    const bar = document.getElementById('progressbar');
    const label = document.getElementById('progresslabel');
    progress.style.display = 'block';
    label.style.display = 'block';
    let width = 1;
    // eslint-disable-next-line no-use-before-define
    const identity = setInterval(scene, interval);
    label.innerHTML = text;

    function scene() {
        if (width >= 100) {
            clearInterval(identity);
            progress.style.display = 'none';
            label.style.display = 'none';
        } else {
            width += 1;
            bar.style.width = `${width}%`;
        }
    }
}
