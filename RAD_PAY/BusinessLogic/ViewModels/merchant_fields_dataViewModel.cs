using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class merchant_fields_dataViewModel
    {
        public int id { get; set; }
        public int? fid { get; set; }
        public int? key { get; set; }
        public string value { get; set; }
        public string prefix { get; set; }
        public int? extra_id { get; set; }
        public int parent_key { get; set; }
        public int service_id { get; set; }
        public int service_id_check { get; set; }
    }
}