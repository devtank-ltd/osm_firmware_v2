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
