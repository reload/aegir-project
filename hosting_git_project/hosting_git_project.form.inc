<?php
/**
 * @file
 * Node forms
 */

/**
 * Implements of hook_form().
 */
function hosting_git_project_form(&$node) {
  $type = node_get_types('type', $node);
  if ($node->type == 'git_project') {
    $form['title'] = array(
      '#type' => 'textfield',
      '#title' => t('Project Name'),
      '#required' => TRUE,
      '#description' => t('Simple name of the project, eg. "Acme Website"'),
      '#size' => 40,
      '#default_value' => $node->title,
      '#maxlength' => 255,
    );

    $form['makefile_url'] = array(
      '#type' => 'textarea',
      '#title' => t('Makefile URL'),
      '#required' => TRUE,
      '#description' => t('URL to the makefile. Make sure to include username and token if the URL is a raw github URL is used and the repository is private.'),
      '#default_value' => $node->makefile_url,
    );

    $form['project_type'] = array(
      '#type' => 'radios',
      '#title' => t('Project Type'),
      '#required' => TRUE,
      '#description' => t('Project type (only makefile is supported currently)'),
      '#default_value' => 'makefile',
      '#options' => array('makefile' => t('Makefile')),
    );


    $form['install_profile'] = array(
      '#type' => 'textfield',
      '#title' => t('Default install profile'),
      '#required' => TRUE,
      '#description' => t('Specify which profile to use when installing sites'),
      '#size' => 40,
      '#default_value' => $node->install_profile,
      '#maxlength' => 255,
    );


    $form['domain_basename'] = array(
      '#type' => 'textfield',
      '#title' => t('Base Domain'),
      '#required' => TRUE,
      '#description' => t('Base-domain to use for naming deployments. A deployment will be named [deployment-name].[base domain]'),
      '#size' => 40,
      '#default_value' => $node->domain_basename,
      '#maxlength' => 255,
    );


    $servers = hosting_get_servers('http');
    $form['web_server'] = array(
      '#type' => 'radios',
      '#title' => t('Web server'),
      '#required' => TRUE,
      '#description' => t('The web server the sites will be hosted on.'),
      '#options' => $servers,
      '#default_value' => ($node->web_server) ? $node->web_server : HOSTING_DEFAULT_WEB_SERVER,
    );

    _hosting_site_field($form, $node, 'db_server', array(
        '#type' => 'radios',
        '#title' => t('Database server'),
        '#required' => TRUE,
        '#description' => t('The database server the site will use to host its content.'),
        '#options' => hosting_get_servers('db'),
        '#default_value' => $node->db_server ? $node->db_server : HOSTING_DEFAULT_DB_SERVER,
      ), '_hosting_node_link');
  }

  if ($node->type == 'git_project_deployment') {
    $form['title'] = array(
      '#type' => 'textfield',
      '#title' => t('Deployment Name'),
      '#required' => TRUE,
      '#description' => t('Simple name of the project, eg. "Nightly" or "Dev"'),
      '#size' => 40,
      '#default_value' => $node->title,
      '#maxlength' => 255,
    );

    $projects = _hosting_git_project_get_projects();
    $form['git_project'] = array(
      '#type' => 'radios',
      '#title' => t('Git Project'),
      '#required' => TRUE,
      '#description' => t('The Git Project this deployment is related to'),
      '#default_value' => $node->git_project ? $node->git_project : '',
      '#options' => $projects,
    );

    $form['deployment_branch'] = array(
      '#type' => 'textfield',
      '#title' => t('Deployment Branch'),
      '#required' => FALSE,
      '#description' => t('Git Branch this deployment should track'),
      '#size' => 40,
      '#maxlength' => 255,
      '#default_value' => $node->deployment_branch ? $node->deployment_branch : 'master',
    );

    $form['deployment_tag'] = array(
      '#type' => 'textfield',
      '#title' => t('Git tag'),
      '#required' => FALSE,
      '#description' => t('Tag this deployment should use'),
      '#size' => 40,
      '#maxlength' => 255,
      '#default_value' => $node->deployment_tag,
    );

    $form['deployment_revision'] = array(
      '#type' => 'textfield',
      '#title' => t('SHA'),
      '#required' => FALSE,
      '#description' => t('SHA this deployment should use.'),
      '#size' => 40,
      '#maxlength' => 255,
      '#default_value' => $node->deployment_revision,
    );
  }


  return $form;
}

/**
 * Validates for platform create.
 */
function hosting_git_project_platform_create_form_validate(&$form, &$form_state) {
  if (empty($form_state['values']['platform_name'])) {
    form_set_error('install_profile', t('You must include a platform name'));
  }
}

/**
 * Implements hook_form_alter().
 */
function hosting_git_project_form_alter(&$form, &$form_state, $form_id) {
  // Removing unneccesary fieldgroups.
  if ($form_id == 'git_project_node_form' || $form_id == 'git_project_deployment_node_form') {
    $form['options']['#access'] = FALSE;
    $form['menu']['#access'] = FALSE;
    $form['revision_information']['#access'] = FALSE;
    $form['author']['#access'] = FALSE;
    $form['comment_settings']['#access'] = FALSE;

    // Because these aren't really posts, 'preview' doesnt make sense in this
    // context.
    $form['buttons']['preview']['#access'] = FALSE;
  }
}
