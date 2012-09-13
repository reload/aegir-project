<?php

/**
 * Implementation of hook_form().
 */
function hosting_project_form(&$node) {
  $type = node_get_types('type', $node);
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
    '#description' => t('"Raw" github url for the makefile including username and token if the url is for a private repository'),
    '#default_value' => $node->makefile_url,
  );

  $form['project_type'] = array(
    '#type' => 'radios',
    '#title' => t('Project Type'),
    '#required' => TRUE,
    '#description' => t('Project type (only Github is supported currently)'),
    '#default_value' => 'github',
    '#options' => array('github' => t('github'))
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


  $form['base_domain'] = array(
    '#type' => 'textfield',
    '#title' => t('Base Domain'),
    '#required' => TRUE,
    '#description' => t('Base-domain to use for naming deployments. A deployment will be named [deployment-name].[base domain]'),
    '#size' => 40,
    '#default_value' => $node->base_domain,
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


  return $form;
}

/**
 * Validates for platform create
 */
function hosting_project_platform_create_form_validate(&$form, &$form_state){
  if (empty($form_state['values']['platform_name'])){
    form_set_error('install_profile', t('You must include a platform name'));
  }
}

/**
 * Implements hook_form_alter().
 */
function hosting_project_form_alter(&$form, &$form_state, $form_id){
  // Removing unneccesary fieldgroups
  if ($form_id == 'project_node_form'){
      $form['options']['#access'] = FALSE;
      $form['menu']['#access'] = FALSE;
      $form['revision_information']['#access'] = FALSE;
      $form['author']['#access'] = FALSE;
      $form['comment_settings']['#access'] = FALSE;

      // because these aren't really posts, 'preview' doesnt make sense in this context.
      $form['buttons']['preview']['#access'] = FALSE;
  }
}


/**
 * Dialog for deployment
 * Implementation of hook_form().
 */
function hosting_task_deploy_form($node) {
  $form['name'] = array (
    '#title' => t('Deployment name'),
    '#description' => t("Name of the deployment"),
    '#type' => 'textfield',
    '#required' => TRUE,
    '#weight' => '-1',
    '#default_value' => strtolower(trim($node->name))
  );


  $form['#node'] = $node;
  return $form;
}


