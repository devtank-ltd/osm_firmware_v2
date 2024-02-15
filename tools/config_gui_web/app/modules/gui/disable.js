export async function disable_interaction(bool) {
  if (typeof bool === 'boolean') {
    document.getElementById('global-fieldset').disabled = bool;
    const nav = document.getElementById('navbar');
    const tags = nav.getElementsByTagName('a');
    if (bool === true) {
      for (let i = 0; i < tags.length; i += 1) {
        tags[i].style.pointerEvents = 'none';
      }
    } else {
      for (let i = 0; i < tags.length; i += 1) {
        tags[i].style.pointerEvents = 'auto';
      }
    }
  } else {
    console.log('Invalid type.');
  }
  return bool;
}

export async function limit_characters(cell, max_len) {
  const cellt = cell.target;
  let text = cellt.innerHTML;
  if (cellt.nodeName === 'INPUT') {
    text = cellt.value;
    if (text.length > max_len) {
      cellt.value = text.slice(0, max_len);
    }
  } else if (text.length > max_len) {
    cellt.innerHTML = text.slice(0, max_len);
  }
}
