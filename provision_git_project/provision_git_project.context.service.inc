<?php
/**
 * @file
 * Contexts used in Git Project.
 */

/**
 * Expose the service type this extension defines to provision.
 *
 * @return array
 *   An array with the service type the key, and the default implementation the
 *   value.
 */
function provision_git_project_provision_services() {
  return array(
    'git_project'            => NULL,
    'git_project_deployment' => NULL,
  );
}

/**
 * The service base class.
 * Hook into provision and make sure all project-related properties are saved
 */
class provisionService_git_project extends provisionService {
  public $service = 'git_project';

  /**
   * Git_project_deployment property should be saved when saving platform aliases
   */
  static function subscribe_platform($context) {
    $context->setProperty('git_project_deployment');
    $context->is_oid('git_project_deployment');
  }
 /**
   * Git_project_deployment property should be saved when saving site aliases
   */
  static function subscribe_site($context) {
    $context->setProperty('git_project_deployment');
    $context->is_oid('git_project_deployment');
  }
}


/**
 * The service base class.
 * Hook into provision and make sure all project-related properties are saved
 */
class provisionService_git_project_deployment extends provisionService {
  public $service = 'git_project_deployment';
}
/**
 * @file Provision named context project class.
 */
class provisionContext_git_project extends provisionContext {

  /**
   * Documentation hook, currently not invoked by provision-save
   */
  static function option_documentation() {
    return array(
      '--project_basename' => 'Project: Project name',
      '--makefile_url' => 'Project: URL to the makefile including token and username if not private repo',
      '--project_name' => 'Project: Name to be used in administrative interfaces',
      '--profile' => 'Project: Installprofile to used',
      '--db_server' => 'Project: The db server project sites will run on',
      '--web_server' => 'Project: The web server project sites will run on',
    );
  }

  // invoked as a project context is initialized, only properties listed here will be read from the list of arguments
  function init_git_project() {
    $this->setProperty('project_name', '');
    $this->setProperty('makefile_url', '');
    $this->setProperty('profile', 'default');
    $this->setProperty('domain_basename', '');
    $this->setProperty('project_type', 'github');
    $this->setProperty('db_server', '@server_localhost');
    $this->setProperty('web_server', '@server_master');

    // specify that the following are subcontexts
    $this->is_oid('db_server');
    $this->is_oid('web_server');
  }
}

/**
 * @file Provision named context project class.
 */
class provisionContext_git_project_deployment extends provisionContext {
  var $parent_key = 'git_project';

  // invoked as a project context is initialized, only properties listed here will be read from the list of arguments
  function init_git_project_deployment() {
    $this->setProperty('deployment_name', '');
    $this->setProperty('deployment_branch', '');
    $this->setProperty('deployment_tag', '');
    $this->setProperty('deployment_revision', '');
    $this->setProperty('status', 'active');
    $this->setProperty('platform', '');
    $this->setProperty('site', '');

   // specify that the following are subcontexts
    $this->is_oid('platform');
    $this->is_oid('site');
   }
}
