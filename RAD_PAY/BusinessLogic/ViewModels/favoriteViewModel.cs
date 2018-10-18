using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class favoriteViewModel
    {
        public int id { get; set; }
        public long? uid { get; set; }
        public int? field_id { get; set; }
        public int? key { get; set; }
        public string value { get; set; }
        public string prefix { get; set; }
        public int? merchant_id { get; set; }
        public string name { get; set; }
        public int? fav_id { get; set; }
    }
}