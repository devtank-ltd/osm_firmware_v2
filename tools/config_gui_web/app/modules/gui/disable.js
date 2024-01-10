export async function disable_interaction(bool) {
  if (typeof bool === 'boolean') {
    document.getElementById('global-fieldset').disabled = bool;
  } else {
    console.log('Invalid type.');
  }
}
