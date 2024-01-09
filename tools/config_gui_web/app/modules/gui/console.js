export class console_t {
  constructor(dev) {
    this.dev = dev;
    this.help_btn = this.help_btn.bind(this);
    this.send_cmd = this.send_cmd.bind(this);
  }

  async open_console() {
    this.doc = document.getElementById('main-page-body');
    this.response = await fetch('modules/gui/html/console.html');
    this.text = await this.response.text();
    this.doc.innerHTML = this.text;
    await this.help_btn();
    await this.bind_input_submit();
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
  }

  async send_cmd() {
    this.cmd = document.getElementById('console-cmd-input');
    const output = await this.dev.do_cmd_raw(this.cmd.value);
    this.terminal = document.getElementById('console-terminal-para');
    this.terminal.textContent = output;
    this.cmd.value = '';
  }
}
