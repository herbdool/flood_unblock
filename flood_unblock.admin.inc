<?php
/**
 * @file
 * Admin page callbacks for the flood_unblock module.
 */

/**
 * The settings form function.
 *
 * TODO: Clean up this code. It's a bit messy. See list below.
 * - String have to be translatable, and it has to be better then it is now!
 * - Split this function up. It is to complex right now.
 */
function flood_unblock_settings() {
  $entries = _flood_unblock_get_entries();
  $config = config('user.flood');
  $ip_limit = $config->get('flood_ip_limit');
  $user_limit = $config->get('flood_user_limit');

  $blocks = array();
  foreach ($entries as $identifier => $entry) {
    $blocks[$identifier] = array(
      'identifier' => $identifier,
      'type'       => $entry['type'],
      'count'      => $entry['count'],
    );
    if ($entry['type'] == 'ip') {
      $blocks[$identifier]['ip'] = $entry['ip'] . $entry['location'];
      $blocks[$identifier]['uid'] = '';
      $blocks[$identifier]['blocked'] = $entry['count'] >= $ip_limit ? "Yes" : "";
    }
    if ($entry['type'] == 'user') {
      $blocks[$identifier]['ip'] = $entry['ip'] . $entry['location'];
      $blocks[$identifier]['uid'] = $entry['username'];
      $blocks[$identifier]['blocked'] = $entry['count'] >= $user_limit ? "Yes" : "";
    }
  }

  $header = array(
    'blocked' => t('Blocked'),
    'type'    => t('Type of block'),
    'count'   => t('Count'),
    'uid'     => t('Account name'),
    'ip'      => t('IP Address'),
  );

  $options = array();
  foreach ($blocks as $block) {
    $options[$block['identifier']] = array(
      'blocked' => $block['blocked'],
      'type'    => $block['type'],
      'count'   => $block['count'],
      'uid'     => $block['uid'],
      'ip'      => $block['ip'],
    );
  }

  $prefix = '<p>There are two ways users are blocked:</p>' .
    '<ul><li>Someone trying to log into the same account repeatedly.</li>' .
    '<li>Multiple login attempts from the same IP address to one or more usernames.</li></ul>' .
    '<p>Both types of blocks has different ';
    $prefix .= l('settings', 'admin/config/people/login') . '</p>';
  

  $form['table'] = array(
    '#type'    => 'tableselect',
    '#header'  => $header,
    '#options' => $options,
    '#empty'   => t('There are no failed logins at this time.'),
    '#prefix'  => t($prefix),
  );

  $form['submit'] = array(
    '#type'  => 'submit',
    '#value' => t('Clear flood'),
  );

  if (count($entries) == 0) {
    $form['submit']['#disabled'] = TRUE;
  }

  return $form;
}

/**
 * The validation function.
 */
function flood_unblock_settings_validate($form, &$form_state) {
  $count = 0;
  foreach ($form_state['values']['table'] as $value) {
    if ($value <> "0") {
      $count++;
    }
  }
  if ($count == 0) {
    form_set_error('', t('Please make a selection.'));
  }
}

/**
 * The submit function.
 */
function flood_unblock_settings_submit($form, &$form_state) {
  foreach ($form_state['values']['table'] as $value) {
    if ($value <> "0") {
      $type = $form['table']['#options'][$value]['type'];
      switch ($type) {
        case 'ip':
          $type = 'failed_login_attempt_ip';
          break;

        case 'user':
          $type = 'failed_login_attempt_user';
          break;

      }

      $identifier = $value;
      flood_unblock_clear_event($type, $identifier);

    }
  }
}

/**
 * Generate rows from the entries in the flood table.
 *
 * @return array
 *   Entries in the flood table.
 */
function  _flood_unblock_get_entries() {
  $entries = array();

  $query = db_select('flood', 'f');
  $query->addField('f', 'identifier');
  $query->addField('f', 'identifier', 'ip');
  $query->addExpression('count(*)', 'count');
  $query->condition('f.event', 'failed_login_attempt_ip');
  $query->groupBy('identifier');

  $results = $query->execute();

  foreach ($results as $result) {
    if (function_exists('smart_ip_get_location')) {
      $location = smart_ip_get_location($result->ip);
      $location_string = sprintf(" (%s %s %s)", $location['city'], $location['region'], $location['country_code']);
    }
    else {
      $location_string = '';
    }
    $entries[$result->identifier] = array(
      'type'     => 'ip',
      'ip'       => $result->ip,
      'count'    => $result->count,
      'location' => $location_string,
    );
  }

  // TODO: Rebuild this query to a dynamic query.
  $sql = "SELECT identifier, count(*) count FROM {flood} WHERE event='failed_login_attempt_user' group by identifier";

  $results = db_query($sql);
  foreach ($results as $result) {
    $parts = explode('-', $result->identifier);
    $result->uid = $parts[0];
    $result->ip = $parts[1];
    if (function_exists('smart_ip_get_location')) {
      $location = smart_ip_get_location($result->ip);
      $location_string = sprintf(" (%s %s %s)", $location['city'], $location['region'], $location['country_code']);
    }
    else {
      $location_string = '';
    }

    $user = user_load($result->uid);
    $entries[$result->identifier] = array(
      'type'     => 'user',
      'uid'      => $result->uid,
      'ip'       => $result->ip,
      'username' => l($user->name, 'user/' . $user->uid),
      'count'    => $result->count,
      'location' => $location_string,
    );
  }

  return $entries;
}
