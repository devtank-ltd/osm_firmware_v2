import { disable_interaction } from './disable.js';

export class console_t {
    constructor(dev) {
        this.dev = dev;
        this.help_btn = this.help_btn.bind(this);
        this.send_cmd = this.send_cmd.bind(this);
    }

    async open_console() {
        await disable_interaction(true);
        this.doc = document.getElementById('main-page-body');
        this.response = await fetch('modules/gui/html/console.html');
        this.text = await this.response.text();
        this.doc.innerHTML = this.text;
        await this.help_btn();
        await this.bind_input_submit();
        await disable_interaction(false);
    }

    async help_btn() {
        this.expand_btn = document.getElementById('con-help');
        this.form_container = document.getElementById('help-container');
        this.help = await this.dev.help();
        this.form_container.innerHTML = this.help;
        this.expand_btn.addEventListener('click', () => {
            this.form_container.style.display = (this.form_container.style.display === 'block') ? 'none' : 'block';
        });
    }

    async bind_input_submit() {
        this.send = document.getElementById('console-send-cmd-btn').addEventListener('click', this.send_cmd);
        this.enter = document.getElementById('console-cmd-input').addEventListener('keyup', this.send_cmd);
    }

    async send_cmd(e) {
        if (e.key === 'Enter' || e.pointerType === 'mouse' || e.type === 'click') {
            await disable_interaction(true);
            this.input = e.target;
            this.text = e.target.value;
            this.cmd = document.getElementById('console-cmd-input');
            this.value = this.cmd.value;
            let output;
            if (this.text || this.value) {
                if (this.value === 'reset') {
                    output = await this.dev.reset();
                    output += '\nReset';
                } else {
                    output = await this.dev.do_cmd_raw(this.value);
                }
                this.terminal = document.getElementById('console-terminal-para');
                this.terminal.textContent = output;
                this.cmd.value = '';
            }
            await disable_interaction(false);
            this.input.focus();
        }
    }
}
